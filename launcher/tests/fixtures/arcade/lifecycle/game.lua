local smoke_written = 0

function init()
  smoke_written = 1
  sprout.storage_set("smoke-written", smoke_written)
  sprout.emit("AchievementUnlocked", "lifecycle-smoke")
end

function update(actions)
end

function render()
  sprout.rect(0, 0, 320, 240, 17, 31, 26)
  sprout.rect(144, 104, 32, 32, 119, 225, 158)
end

function snapshot()
  return "lifecycle:" .. tostring(smoke_written)
end
