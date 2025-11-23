-- Motion Playback Plugin for Xournal++
-- This plugin demonstrates how to access motion recording data
-- and export it as frames for video rendering

-- Register all Toolbar actions and initialize all UI stuff
function initUi()
  app.registerUi({
    ["menu"] = "Export Motion Recording",
    ["callback"] = "exportMotionRecording",
    ["accelerator"] = "<Shift><Alt>m"
  })
  print("Motion Playback plugin registered\n")
end

-- Main callback function
function exportMotionRecording()
  print("Motion Recording Export started\n")
  
  -- Get document structure
  local docStructure = app.getDocumentStructure()
  local xoppFilename = docStructure["xoppFilename"]
  
  if xoppFilename == "" or xoppFilename == nil then
    app.openDialog("Please save your document first before exporting motion recording.", {"OK"}, "", true)
    return
  end
  
  -- Get output directory from user
  app.fileDialogSave("exportMotionRecordingCallback", getStem(xoppFilename) .. "_motion_frames")
end

-- Callback after user selects output path
function exportMotionRecordingCallback(outputPath)
  if outputPath == "" then
    print("Motion export cancelled by user\n")
    return
  end
  
  print("Exporting motion recording to: " .. outputPath .. "\n")
  
  -- Get all strokes with motion recording data
  local docStructure = app.getDocumentStructure()
  local pages = docStructure["pages"]
  
  local strokesWithMotion = 0
  local totalMotionPoints = 0
  
  -- Count strokes with motion data
  for pageNum, page in ipairs(pages) do
    local layers = page["layers"]
    for layerNum, layer in ipairs(layers) do
      local elements = layer["elements"]
      for elemNum, element in ipairs(elements) do
        if element["type"] == "stroke" then
          -- Check if stroke has motion recording
          -- Note: This would require extending the Lua API to expose motion recording data
          -- For now, we'll document the expected API
          if element["hasMotionRecording"] then
            strokesWithMotion = strokesWithMotion + 1
            if element["motionPoints"] then
              totalMotionPoints = totalMotionPoints + #element["motionPoints"]
            end
          end
        end
      end
    end
  end
  
  local message = string.format(
    "Motion Recording Export Summary:\n\n" ..
    "Strokes with motion data: %d\n" ..
    "Total motion points: %d\n\n" ..
    "Note: Full export functionality requires extending the Lua API to expose motion recording data.\n\n" ..
    "Expected output format:\n" ..
    "- Frame images showing pen/eraser positions\n" ..
    "- Metadata file with timestamps\n" ..
    "- Can be converted to video using external tools",
    strokesWithMotion, totalMotionPoints
  )
  
  app.openDialog(message, {"OK"}, "", false)
  
  print(string.format("Found %d strokes with motion recording\n", strokesWithMotion))
end

-- Helper function to get file stem (without extension)
function getStem(filename)
  if filename == "" or filename == nil then
    return "untitled"
  end
  return filename:match("(.+)%..+$") or filename
end
