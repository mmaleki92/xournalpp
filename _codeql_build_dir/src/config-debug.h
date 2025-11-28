/*
 * Xournal++
 *
 * Static debugging definitions for Xournal++
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

/**
 * Input debugging, e.g. eraser events etc.
 */
/* #undef DEBUG_INPUT */
/* #undef DEBUG_INPUT_GDK_PRINT_EVENTS */
/* #undef DEBUG_INPUT_PRINT_ALL_MOTION_EVENTS */

/**
 * Shape recognizer debug: output score etc.
 */
/* #undef DEBUG_RECOGNIZER */

/**
 * Scheduler debug: show jobs etc.
 */
/* #undef DEBUG_SCHEDULER */

/**
 * Draw a surrounding border to all elements
 */
/* #undef DEBUG_SHOW_ELEMENT_BOUNDS */

/**
 * Draw a border around all repaint rects
 */
/* #undef DEBUG_SHOW_REPAINT_BOUNDS */

/**
 * Draw a border around all painted rects
 */
/* #undef DEBUG_SHOW_PAINT_BOUNDS */

/**
 * Draw the mask in StrokeView
 */
/* #undef DEBUG_MASKS */

/**
 * Print info on draw requests and calls
 */
/* #undef DEBUG_DRAW_WIDGET */

/**
 * Show repainted areas when erasing an highlighter stroke
 */
/* #undef DEBUG_ERASABLE_STROKE_BOXES */

/**
 * Show eraser logging info when we get an inconsistent result
 */
/* #undef ENABLE_ERASER_DEBUG */

/**
 * Enable some debugging output around the action database
 */
/* #undef DEBUG_ACTION_DB */
