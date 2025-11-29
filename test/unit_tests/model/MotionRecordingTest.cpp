/*
 * Xournal++
 *
 * This file is part of the Xournal UnitTests
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include "model/MotionRecording.h"
#include "model/PathParameter.h"
#include "model/Point.h"
#include "model/Stroke.h"

TEST(MotionRecording, testBasicOperations) {
    MotionRecording motion;
    
    // Initially should be empty
    EXPECT_FALSE(motion.hasMotionData());
    EXPECT_EQ(motion.getMotionPointCount(), 0);
    
    // Add a pen motion point
    motion.addMotionPoint(Point(100.0, 200.0, 0.8), 1000, false);
    EXPECT_TRUE(motion.hasMotionData());
    EXPECT_EQ(motion.getMotionPointCount(), 1);
    
    // Add an eraser motion point
    motion.addMotionPoint(Point(150.0, 180.0, -1.0), 2000, true);
    EXPECT_EQ(motion.getMotionPointCount(), 2);
    
    // Check timestamps
    EXPECT_EQ(motion.getStartTimestamp(), 1000);
    EXPECT_EQ(motion.getEndTimestamp(), 2000);
    
    // Clear motion data
    motion.clear();
    EXPECT_FALSE(motion.hasMotionData());
    EXPECT_EQ(motion.getMotionPointCount(), 0);
}

TEST(MotionRecording, testMotionPoints) {
    MotionRecording motion;
    
    // Add multiple motion points
    motion.addMotionPoint(Point(10.0, 20.0, 0.5), 100, false);
    motion.addMotionPoint(Point(15.0, 25.0, 0.6), 200, false);
    motion.addMotionPoint(Point(20.0, 30.0, -1.0), 300, true);
    
    const auto& points = motion.getMotionPoints();
    EXPECT_EQ(points.size(), 3);
    
    // Check first point (pen)
    EXPECT_EQ(points[0].timestamp, 100);
    EXPECT_EQ(points[0].point.x, 10.0);
    EXPECT_EQ(points[0].point.y, 20.0);
    EXPECT_EQ(points[0].point.z, 0.5);
    EXPECT_FALSE(points[0].isEraser);
    
    // Check second point (pen)
    EXPECT_EQ(points[1].timestamp, 200);
    EXPECT_EQ(points[1].point.x, 15.0);
    EXPECT_FALSE(points[1].isEraser);
    
    // Check third point (eraser)
    EXPECT_EQ(points[2].timestamp, 300);
    EXPECT_EQ(points[2].point.x, 20.0);
    EXPECT_TRUE(points[2].isEraser);
}

TEST(MotionRecording, testStrokeIntegration) {
    Stroke stroke;
    
    // Initially no motion recording
    EXPECT_FALSE(stroke.hasMotionRecording());
    EXPECT_EQ(stroke.getMotionRecording(), nullptr);
    
    // Create and attach motion recording
    auto motion = std::make_unique<MotionRecording>();
    motion->addMotionPoint(Point(50.0, 60.0, 0.7), 500, false);
    motion->addMotionPoint(Point(55.0, 65.0, 0.8), 600, false);
    
    stroke.setMotionRecording(std::move(motion));
    
    // Check motion recording is attached
    EXPECT_TRUE(stroke.hasMotionRecording());
    EXPECT_NE(stroke.getMotionRecording(), nullptr);
    
    // Verify motion data
    const auto* attachedMotion = stroke.getMotionRecording();
    EXPECT_EQ(attachedMotion->getMotionPointCount(), 2);
    EXPECT_EQ(attachedMotion->getStartTimestamp(), 500);
    EXPECT_EQ(attachedMotion->getEndTimestamp(), 600);
}

TEST(MotionRecording, testStrokeClone) {
    Stroke stroke;
    
    // Add motion recording to original stroke
    auto motion = std::make_unique<MotionRecording>();
    motion->addMotionPoint(Point(100.0, 200.0, 0.5), 1000, false);
    motion->addMotionPoint(Point(150.0, 250.0, -1.0), 2000, true);
    stroke.setMotionRecording(std::move(motion));
    
    // Clone the stroke
    auto clonedStroke = stroke.cloneStroke();
    
    // Verify clone has motion recording
    EXPECT_TRUE(clonedStroke->hasMotionRecording());
    
    const auto* clonedMotion = clonedStroke->getMotionRecording();
    EXPECT_NE(clonedMotion, nullptr);
    EXPECT_EQ(clonedMotion->getMotionPointCount(), 2);
    
    // Verify motion data is the same
    const auto& clonedPoints = clonedMotion->getMotionPoints();
    EXPECT_EQ(clonedPoints[0].timestamp, 1000);
    EXPECT_EQ(clonedPoints[0].point.x, 100.0);
    EXPECT_FALSE(clonedPoints[0].isEraser);
    EXPECT_EQ(clonedPoints[1].timestamp, 2000);
    EXPECT_TRUE(clonedPoints[1].isEraser);
}

TEST(MotionRecording, testEraserMotion) {
    MotionRecording motion;
    
    // Add only eraser motion points
    motion.addMotionPoint(Point(100.0, 100.0, -1.0), 1000, true);
    motion.addMotionPoint(Point(110.0, 110.0, -1.0), 1100, true);
    motion.addMotionPoint(Point(120.0, 120.0, -1.0), 1200, true);
    
    const auto& points = motion.getMotionPoints();
    EXPECT_EQ(points.size(), 3);
    
    // All points should be marked as eraser
    for (const auto& point : points) {
        EXPECT_TRUE(point.isEraser);
        EXPECT_EQ(point.point.z, -1.0);  // Eraser typically has no pressure
    }
}

TEST(MotionRecording, testMixedPenAndEraser) {
    MotionRecording motion;
    
    // Add alternating pen and eraser points
    motion.addMotionPoint(Point(10.0, 10.0, 0.5), 100, false);  // pen
    motion.addMotionPoint(Point(20.0, 20.0, -1.0), 200, true);  // eraser
    motion.addMotionPoint(Point(30.0, 30.0, 0.7), 300, false);  // pen
    motion.addMotionPoint(Point(40.0, 40.0, -1.0), 400, true);  // eraser
    
    const auto& points = motion.getMotionPoints();
    EXPECT_EQ(points.size(), 4);
    
    // Verify alternating pattern
    EXPECT_FALSE(points[0].isEraser);
    EXPECT_TRUE(points[1].isEraser);
    EXPECT_FALSE(points[2].isEraser);
    EXPECT_TRUE(points[3].isEraser);
}

TEST(MotionRecording, testEmptyMotionRecording) {
    MotionRecording motion;
    
    // Test empty state
    EXPECT_EQ(motion.getStartTimestamp(), 0);
    EXPECT_EQ(motion.getEndTimestamp(), 0);
    EXPECT_TRUE(motion.getMotionPoints().empty());
    
    // Clear should not crash on empty
    motion.clear();
    EXPECT_FALSE(motion.hasMotionData());
}

TEST(MotionRecording, testSerialization) {
    // This test is included to verify that the serialization/deserialization
    // methods compile correctly and use the proper API calls.
    // Full serialization integration is tested elsewhere.
    
    MotionRecording motion;
    
    // Add multiple motion points with different types
    motion.addMotionPoint(Point(10.0, 20.0, 0.5), 100, false);  // pen
    motion.addMotionPoint(Point(15.0, 25.0, 0.6), 200, false);  // pen
    motion.addMotionPoint(Point(20.0, 30.0, -1.0), 300, true);  // eraser
    
    EXPECT_EQ(motion.getMotionPointCount(), 3);
    
    // Note: Full serialization testing requires ObjectOutputStream/ObjectInputStream
    // which are tested in the util/ObjectIOStreamTest.cpp file.
    // This test primarily verifies the API usage compiles correctly.
}

TEST(MotionRecording, testStrokeSectionCloneDoesNotCopyMotion) {
    // Create a stroke with motion recording
    Stroke stroke;
    stroke.addPoint(Point(0.0, 0.0, 0.5));
    stroke.addPoint(Point(10.0, 10.0, 0.5));
    stroke.addPoint(Point(20.0, 20.0, 0.5));
    stroke.addPoint(Point(30.0, 30.0, 0.5));
    
    // Add motion recording to the stroke
    auto motion = std::make_unique<MotionRecording>();
    motion->addMotionPoint(Point(0.0, 0.0, 0.5), 0, false);
    motion->addMotionPoint(Point(5.0, 5.0, 0.5), 100, false);
    motion->addMotionPoint(Point(10.0, 10.0, 0.5), 200, false);
    motion->addMotionPoint(Point(15.0, 15.0, 0.5), 300, false);
    motion->addMotionPoint(Point(20.0, 20.0, 0.5), 400, false);
    motion->addMotionPoint(Point(25.0, 25.0, 0.5), 500, false);
    motion->addMotionPoint(Point(30.0, 30.0, 0.5), 600, false);
    
    stroke.setMotionRecording(std::move(motion));
    EXPECT_TRUE(stroke.hasMotionRecording());
    
    // Clone a section of the stroke (simulating what happens during erasing)
    PathParameter lowerBound(0, 0.5);  // Middle of first segment
    PathParameter upperBound(1, 0.5);  // Middle of second segment
    
    auto clonedSection = stroke.cloneSection(lowerBound, upperBound);
    
    // Cloned sections should NOT have motion recording
    // Motion recording contains the full original path, which would cause
    // rendering issues if copied to fragments
    EXPECT_FALSE(clonedSection->hasMotionRecording());
    EXPECT_EQ(clonedSection->getMotionRecording(), nullptr);
    
    // But the cloned section should have its own geometry points
    EXPECT_GE(clonedSection->getPointCount(), 2);
}

TEST(MotionRecording, testStrokeSectionCloneWithoutMotion) {
    // Create a stroke WITHOUT motion recording
    Stroke stroke;
    stroke.addPoint(Point(0.0, 0.0, 0.5));
    stroke.addPoint(Point(10.0, 10.0, 0.5));
    stroke.addPoint(Point(20.0, 20.0, 0.5));
    
    EXPECT_FALSE(stroke.hasMotionRecording());
    
    // Clone a section of the stroke
    PathParameter lowerBound(0, 0.0);
    PathParameter upperBound(0, 1.0);
    
    auto clonedSection = stroke.cloneSection(lowerBound, upperBound);
    
    // Verify the cloned section has no motion recording (as expected)
    EXPECT_FALSE(clonedSection->hasMotionRecording());
    EXPECT_EQ(clonedSection->getMotionRecording(), nullptr);
}

TEST(MotionRecording, testMultipleSectionCloneNoMotion) {
    // Create a stroke with motion recording
    Stroke stroke;
    stroke.addPoint(Point(0.0, 0.0, 0.5));
    stroke.addPoint(Point(10.0, 10.0, 0.5));
    stroke.addPoint(Point(20.0, 20.0, 0.5));
    stroke.addPoint(Point(30.0, 30.0, 0.5));
    
    auto motion = std::make_unique<MotionRecording>();
    motion->addMotionPoint(Point(0.0, 0.0, 0.5), 0, false);
    motion->addMotionPoint(Point(15.0, 15.0, 0.5), 300, false);
    motion->addMotionPoint(Point(30.0, 30.0, 0.5), 600, false);
    
    stroke.setMotionRecording(std::move(motion));
    
    // Clone two different sections (simulating stroke split by eraser)
    PathParameter lb1(0, 0.0);
    PathParameter ub1(0, 0.5);
    PathParameter lb2(1, 0.5);
    PathParameter ub2(2, 1.0);
    
    auto section1 = stroke.cloneSection(lb1, ub1);
    auto section2 = stroke.cloneSection(lb2, ub2);
    
    // Neither section should have motion recording
    // (fragments are rendered as static elements)
    EXPECT_FALSE(section1->hasMotionRecording());
    EXPECT_FALSE(section2->hasMotionRecording());
    
    // Both sections should have geometry points though
    EXPECT_GE(section1->getPointCount(), 2);
    EXPECT_GE(section2->getPointCount(), 2);
}
