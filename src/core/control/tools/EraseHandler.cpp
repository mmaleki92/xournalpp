#include "EraseHandler.h"

#include <memory>   // for make_unique, unique_ptr
#include <utility>  // for move
#include <vector>   // for vector

#include <gdk/gdk.h>  // for GdkRectangle
#include <glib.h>     // for gint, g_get_monotonic_time

#include "control/ToolEnums.h"            // for ERASER_TYPE_DELETE_STROKE
#include "control/ToolHandler.h"          // for ToolHandler
#include "gui/LegacyRedrawable.h"         // for Redrawable
#include "model/Document.h"               // for Document
#include "model/Element.h"                // for Element, ELEMENT_STROKE
#include "model/Layer.h"                  // for Layer
#include "model/MotionRecording.h"        // for MotionRecording
#include "model/Point.h"                  // for Point
#include "model/Stroke.h"                 // for Stroke
#include "model/XojPage.h"                // for XojPage
#include "model/eraser/ErasableStroke.h"  // for ErasableStroke
#include "model/eraser/PaddedBox.h"       // for PaddedBox
#include "undo/DeleteUndoAction.h"        // for DeleteUndoAction
#include "undo/EraseUndoAction.h"         // for EraseUndoAction
#include "undo/UndoRedoHandler.h"         // for UndoRedoHandler
#include "util/Color.h"                   // for Colors
#include "util/Range.h"                   // for Range
#include "util/SmallVector.h"             // for SmallVector
#include "util/safe_casts.h"              // for as_unsigned

namespace {
// Convert eraser thickness (radius) to diameter for motion recording
constexpr double ERASER_RADIUS_TO_DIAMETER = 2.0;
// Convert microseconds to milliseconds
constexpr int64_t MICROSECONDS_TO_MILLISECONDS = 1000;
}  // namespace

EraseHandler::EraseHandler(UndoRedoHandler* undo, Document* doc, const PageRef& page, ToolHandler* handler,
                           LegacyRedrawable* view):
        page(page),
        handler(handler),
        view(view),
        doc(doc),
        undo(undo),
        eraseDeleteUndoAction(nullptr),
        eraseUndoAction(nullptr),
        halfEraserSize(0),
        eraserMotionStroke(nullptr),
        eraserMotionRecording(nullptr) {}

EraseHandler::~EraseHandler() {
    if (this->eraseDeleteUndoAction) {
        this->finalize();
    }
    // Clean up eraser motion stroke if it wasn't finalized
    eraserMotionStroke.reset();
    eraserMotionRecording.reset();
}

/**
 * Handle eraser event: "Delete Stroke" and "Standard", Whiteout is not handled here
 */
void EraseHandler::erase(double x, double y, size_t timestamp) {
    this->halfEraserSize = this->handler->getThickness();
    GdkRectangle eraserRect = {gint(x - halfEraserSize), gint(y - halfEraserSize), gint(halfEraserSize * 2),
                               gint(halfEraserSize * 2)};

    Range range(x, y);

    Layer* l = page->getSelectedLayer();

    // Initialize eraser motion recording if this is the first erase call
    if (!eraserMotionRecording) {
        eraserMotionRecording = std::make_unique<MotionRecording>();
        
        // Create a stroke to hold the eraser motion
        eraserMotionStroke = std::make_unique<Stroke>();
        eraserMotionStroke->setToolType(StrokeTool::ERASER);
        // Convert eraser radius (thickness) to diameter for the motion stroke
        eraserMotionStroke->setWidth(this->handler->getThickness() * ERASER_RADIUS_TO_DIAMETER);
        eraserMotionStroke->setColor(Colors::white);  // White color for eraser
    }

    // Record eraser motion point
    // Use current timestamp or generate one if not provided
    size_t currentTimestamp = timestamp;
    if (currentTimestamp == 0) {
        // Convert microseconds to milliseconds for timestamp
        currentTimestamp = as_unsigned(g_get_monotonic_time() / MICROSECONDS_TO_MILLISECONDS);
    }
    
    Point eraserPoint(x, y, -1.0);  // No pressure for eraser
    eraserMotionRecording->addMotionPoint(eraserPoint, currentTimestamp, true);  // isEraser = true

    for (Element* e: xoj::refElementContainer(l->getElements())) {
        if (e->getType() == ELEMENT_STROKE && e->intersectsArea(&eraserRect)) {
            eraseStroke(l, dynamic_cast<Stroke*>(e), x, y, range);
        }
    }

    this->view->rerenderRange(range);
}

void EraseHandler::eraseStroke(Layer* l, Stroke* s, double x, double y, Range& range) {
    ErasableStroke* erasable = s->getErasable();
    if (!erasable) {
        if (this->handler->getEraserType() == ERASER_TYPE_DELETE_STROKE) {
            if (!s->intersects(x, y, halfEraserSize)) {
                // The stroke does not intersect the eraser square
                return;
            }

            // delete the entire stroke
            this->doc->lock();
            auto [stroke, pos] = l->removeElement(s);
            this->doc->unlock();

            if (pos == -1) {
                return;
            }
            range.addPoint(s->getX(), s->getY());
            range.addPoint(s->getX() + s->getElementWidth(), s->getY() + s->getElementHeight());

            // removed the if statement - this prevents us from putting multiple elements into a
            // stroke erase operation, but it also prevents the crashing and layer issues!
            if (!this->eraseDeleteUndoAction) {
                auto eraseDel = std::make_unique<DeleteUndoAction>(this->page, true);
                // Todo check dangerous: this->eraseDeleteUndoAction could be a dangling reference
                this->eraseDeleteUndoAction = eraseDel.get();
                this->undo->addUndoAction(std::move(eraseDel));
            }

            this->eraseDeleteUndoAction->addElement(l, std::move(stroke), pos);
        } else {  // Default eraser
            auto pos = l->indexOf(s);
            if (pos == -1) {
                return;
            }

            const double paddingCoeff = PADDING_COEFFICIENT_CAP[s->getStrokeCapStyle()];
            const PaddedBox paddedEraserBox{{x, y}, halfEraserSize, halfEraserSize + paddingCoeff * s->getWidth()};
            auto intersectionParameters = s->intersectWithPaddedBox(paddedEraserBox);

            if (intersectionParameters.empty()) {
                // The stroke does not intersect the eraser square
                return;
            }

            if (this->eraseUndoAction == nullptr) {
                auto eraseUndo = std::make_unique<EraseUndoAction>(this->page);
                // Todo check dangerous: this->eraseDeleteUndoAction could be a dangling reference
                this->eraseUndoAction = eraseUndo.get();
                this->undo->addUndoAction(std::move(eraseUndo));
            }

            doc->lock();
            erasable = new ErasableStroke(*s);
            s->setErasable(erasable);
            doc->unlock();
            this->eraseUndoAction->addOriginal(l, s, pos);
            erasable->beginErasure(intersectionParameters, range);
        }
    } else {
        /**
         * This stroke has already been touched by the eraser
         * (Necessarily the default eraser)
         */
        auto pos = l->indexOf(s);
        if (pos == -1) {
            return;
        }
        const double paddingCoeff = PADDING_COEFFICIENT_CAP[s->getStrokeCapStyle()];
        const PaddedBox paddedEraserBox{{x, y}, halfEraserSize, halfEraserSize + paddingCoeff * s->getWidth()};
        erasable->erase(paddedEraserBox, range);
    }
}

void EraseHandler::finalize() {
    if (this->eraseUndoAction) {
        this->eraseUndoAction->finalize();
        this->eraseUndoAction = nullptr;
    } else if (this->eraseDeleteUndoAction) {
        this->eraseDeleteUndoAction = nullptr;
    }
    
    // Add eraser motion stroke to the page if we recorded any motion
    if (eraserMotionRecording && eraserMotionStroke) {
        // Attach motion recording to the eraser stroke
        eraserMotionStroke->setMotionRecording(std::move(eraserMotionRecording));
        
        // Add the eraser motion stroke to the page
        Layer* l = page->getSelectedLayer();
        if (l) {
            doc->lock();
            l->addElement(std::move(eraserMotionStroke));
            doc->unlock();
        } else {
            // Warn if we can't add the eraser motion stroke
            g_warning("Cannot add eraser motion stroke: no selected layer");
        }
    }
    
    // Clean up
    eraserMotionStroke.reset();
    eraserMotionRecording.reset();
}
