local tab = require 'tabutil'

local Hook = {}
Hook.__index = Hook

function Hook.new()
  local this = {}
  this.callbacks = {}
  return setmetatable(this, Hook)
end

function Hook:register(name, func)
  self.callbacks[name] = func
end

function Hook:deregister(name)
  self.callbacks[name] = nil
end

function Hook:__call(...)
  for k, func in pairs(self.callbacks) do
    if func ~= nil then
      print('calling: ' .. k)
      local ok, error = pcall(func, arg)
      if not ok then
        print('hook: ' .. k .. ' failed, error: ' .. error)
      end
    end
  end
end

-- define the hook types
local hooks = {
  script_pre_init = Hook.new(),
  script_post_cleanup = Hook.new(),
  audio_post_restore_default_routing = Hook.new(),
  system_post_startup = Hook.new(),
  system_pre_shutdown = Hook.new(),
}

return tab.readonly{table = hooks}
