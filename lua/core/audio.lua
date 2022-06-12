--- Audio class
-- @module audio

local cs = require 'controlspec'
local hook = require 'core/hook'

local Audio = {}

--- set headphone gain.
-- @tparam number gain (0-64)
Audio.headphone_gain = function(gain)
  _norns.gain_hp(gain)
end

--- set level for ADC input.
-- @param level in [0, 1]
Audio.level_adc = function(level)
  _norns.level_adc(level)
end

--- set level for *both* output channels.
-- @param level in [0, 1]
Audio.level_dac = function(level)
  _norns.level_dac(level)
end

-- @static
Audio.level_eng = function(level)
  _norns.level_ext(level)
end

--- set monitor level for *both* input channels.
-- @param level in [0, 1]
Audio.level_monitor = function(level)
  _norns.level_monitor(level)
end

--- set monitor mode to mono.
--- both inputs will be mixed to both outputs.
Audio.monitor_mono = function()
  _norns.monitor_mix_mono()
end

--- set monitor mode to stereo.
--- each input will be monitored on the respective output.
Audio.monitor_stereo = function()
  _norns.monitor_mix_stereo()
end

--- set tape level.
-- @param level [0,1]
Audio.level_tape = function(level)
  _norns.level_tape(level)
end

--- set cut master level.
-- @param level [0,1]
Audio.level_cut = function(level)
  _norns.level_cut_master(level)
end

--- enable input pitch analysis.
Audio.pitch_on = function()
  _norns.audio_pitch_on()
end

--- disable input pitch analysis (saves CPU).
Audio.pitch_off = function()
  _norns.audio_pitch_off()
end

--- restart the audio engine (recompile sclang).
Audio.restart = function()
   _norns.restart_audio()
end



--- Effects functions
-- @section Effects

--- reverb on.
function Audio.rev_on()
   _norns.rev_on()
end

--- reverb off.
function Audio.rev_off()
   _norns.rev_off()
end

--- reverb Monitor level.
-- @tparam number val
function Audio.level_monitor_rev(val)
   _norns.level_monitor_rev(val)
end

--- reverb ENGINE level.
-- @tparam number val
function Audio.level_eng_rev(val)
   _norns.level_ext_rev(val)
end

--- reverb TAPE level.
-- @tparam number val
function Audio.level_tape_rev(val)
   _norns.level_tape_rev(val)
end

--- reverb DAC level.
-- @tparam number val
function Audio.level_rev_dac(val)
   _norns.level_rev_dac(val)
end

--- set reverb parameter.
-- @tparam string name
-- @tparam number val
function Audio.rev_param(name, val)
   _norns.rev_param(name, val)
end

--- turn on compressor.
function Audio.comp_on()
   _norns.comp_on()
end

--- turn off compressor.
function Audio.comp_off()
   _norns.comp_off()
end

--- compressor mix amount.
-- @tparam number val
function Audio.comp_mix(val)
   _norns.comp_mix(val)
end

--- set compressor parameter.
-- @tparam string name
-- @tparam number val
function Audio.comp_param(name, val)
   _norns.comp_param(name, val)
end



--- Tape Functions
-- @section Tape

--- open a tape file.
-- @param file
Audio.tape_play_open = function(file)
  _norns.tape_play_open(file)
end

--- start tape playing.
Audio.tape_play_start = function()
  _norns.tape_play_start()
end

--- stop tape playing.
Audio.tape_play_stop = function()
  _norns.tape_play_stop()
end

--- open a tape recording file.
-- @param file
Audio.tape_record_open = function(file)
  _norns.tape_record_open(file)
end

--- start tape recording.
Audio.tape_record_start = function()
  _norns.tape_record_start()
end

--- stop tape recording.
Audio.tape_record_stop = function()
  _norns.tape_record_stop()
end


--- Softcut levels
-- @section softcut

--- softcut adc level.
-- @tparam number value
Audio.level_adc_cut = function(value)
  _norns.level_adc_cut(value)
end

--- softcut eng level.
-- @tparam number value
Audio.level_eng_cut = function(value)
  _norns.level_ext_cut(value)
end

--- softcut tape level.
-- @tparam number value
Audio.level_tape_cut = function(value)
  _norns.level_tape_cut(value)
end

--- softcut cut reverb level.
-- @tparam number value
Audio.level_cut_rev = function(value)
  _norns.level_cut_rev(value)
end



--- Routing Functions
-- @section Routing

--- default audio connections for softcut
Audio.default_softcut_routes = {
  ["softcut:output_1"] = "crone:input_3",
  ["softcut:output_2"] = "crone:input_4",
  ["crone:output_3"]   = "softcut:input_1",
  ["crone:output_4"]   = "softcut:input_2",
}

--- default audio connections for supercollider
Audio.default_supercollider_routes = {
  ["SuperCollider:out_1"] = "crone:input_5",
  ["SuperCollider:out_2"] = "crone:input_6",
  ["crone:output_5"]      = "SuperCollider:in_1",
  ["crone:output_6"]      = "SuperCollider:in_2",
}

--- default audio connections for system (hardware)
Audio.default_system_routes = {
  ["system:capture_1"] = "crone:input_1",
  ["system:capture_2"] = "crone:input_2",
  ["crone:output_1"]   = "system:playback_1",
  ["crone:output_2"]   = "system:playback_2",
}

Audio._default_routing_altered = false

--- return the names of all input ports
-- @treturn table list of port names
Audio.input_ports = function()
  return _norns.audio_get_input_ports()
end

--- return the names of all output ports
-- @treturn table list of port names
Audio.output_ports = function()
  return _norns.audio_get_output_ports()
end

--- lookup all connections to the given port
--
-- the provided port name can be either an input port of an output port. if an
-- input port is given then the returned list will consisten of output ports. if
-- an output port is specified, connected input ports are returned.
--
-- @tparam string port name
-- @treturn table list of port names
Audio.port_connections = function(port)
  return _norns.audio_get_port_connections(port)
end

--- connect an input to output or output to input
-- @tparam string from the initiating port
-- @tparam string to the desitnation port
-- @treturn boolean true if connection request altered any routing
Audio.port_connect = function(from, to)
  local changed = _norns.audio_connect(from, to)
  Audio._default_routing_altered = Audio._default_routing_altered or changed
  return changed
end

--- disconnect an input to output or output to input
-- @tparam string from the initiating port
-- @tparam string to the desitnation port
-- @treturn boolean true if connection request altered any routing
Audio.port_disconnect = function(from, to)
  local changed = _norns.audio_disconnect(from, to)
  Audio._default_routing_altered = Audio._default_routing_altered or changed
  return changed
end

--- determine if the audio routing has been altered via port_connect or
--  port_disconnect
-- @treturn boolean true if connections have been altered
Audio.routing_is_altered = function()
  -- NOTE: this works as long as port_connect and port_disconnect are used
  -- exclusively. a more robust but slower implementation could use
  -- port_connections to query all routes and compare those to the default
  -- routing tables.
  return Audio._default_routing_altered
end

--- disconnect all routing between audio clients
Audio.routing_disconnect_all = function()
  -- input ports
  for _, ip in ipairs(_norns.audio_get_input_ports()) do
    for _, c in ipairs(_norns.audio_get_port_connections(ip)) do
      _norns.audio_disconnect(ip, c)
    end
  end
  -- output ports (is this redundant?)
  for _, op in ipairs(_norns.audio_get_output_ports()) do
    for _, c in ipairs(_norns.audio_get_port_connections(op)) do
      _norns.audio_disconnect(op, c)
    end
  end
  -- mark routing as altered since we are using the low level functions
  Audio._default_routing_altered = true
end

--- restores default audio routing
--
-- disconnects all established audio connections regardless of how they were
-- created and establishes the default connections for the system, softcut, and
-- supercollider.
Audio.routing_default = function()
  -- start from a known state
  Audio.routing_disconnect_all()
  Audio.system_connect()
  Audio.softcut_connect()
  Audio.supercollider_connect()

  -- allow mods to tweak defaults
  hook.audio_post_restore_default_routing()

  Audio._default_routing_altered = false
end

Audio._routing_apply = function(routes, operation)
  for source, destination in pairs(routes) do
    operation(source, destination)
  end
end

--- connect all routes defined in the given table
--
-- routing table keys specify the source port name with the associated value
-- defining the destination port name.
--
-- @tparam table routes connections to establish
Audio.routing_connect = function(routes)
  Audio._routing_apply(routes, Audio.port_connect)
end

--- disconnect all routes defined in the given table
--
-- routing table keys specify the source port name with the associated value
-- defining the destination port name.
--
-- @tparam table routes connections to establish
Audio.routing_disconnect = function(routes)
  Audio._routing_apply(routes, Audio.port_disconnect)
end

--- establish with default connections for system (hardware) input and output
Audio.system_connect = function()
  Audio.routing_connect(Audio.default_system_routes)
end

--- remove default connections for system (hardware) input and output
Audio.system_disconnect = function()
  Audio.routing_disconnect(Audio.default_system_routes)
end

--- establish with default connections for supercollider input and output
Audio.supercollider_connect = function()
  Audio.routing_connect(Audio.default_supercollider_routes)
end

--- remove default connections for supercollider input and output
Audio.supercollider_disconnect = function()
  Audio.routing_disconnect(Audio.default_supercollider_routes)
end

--- establish with default connections for softcut input and output
Audio.softcut_connect = function()
  Audio.routing_connect(Audio.default_softcut_routes)
end

--- remove default connections for softcut input and output
Audio.softcut_disconnect = function()
  Audio.routing_disconnect(Audio.default_softcut_routes)
end

--- prevents changes from triggering post script clean of audio routing
--
--  all audio routing performed within the passed in function is consider to
--  have had no effect on the default system routing. disabling change tracking
--  can be useful in mods and other specialized circumstances where changes need
--  to persist across scripts.
Audio.with_change_tracking_disabled = function(f)
  local result = f()
  Audio._default_routing_altered = false
  return result
end



--- global functions
-- @section globals

--- callback for VU meters.
-- scripts should redefine this.
-- @param in1 input level 1 in [0, 63], audio taper
-- @param in2
-- @param out1
-- @param out2
Audio.vu = function(in1, in2, out1, out2)
   -- print (in1 .. '\t' .. in2 .. '\t' .. out1 .. '\t' .. out2)
end


--- helpers
-- @section helpers

--- set output level, clamped, save state.
-- @tparam number value audio level (0-64)
function Audio.set_audio_level(value)
  local l = util.clamp(value,0,64)
  if l ~= norns.state.out then
    norns.state.out = l
    Audio.output_level(l / 64.0)
  end
end

--- adjust output level, clamped, save state.
-- @tparam number delta amount to change output level
function Audio.adjust_output_level(delta)
  local l = util.clamp(norns.state.out + delta,0,64)
  if l ~= norns.state.out then
    norns.state.out = l
    Audio.output_level(l / 64.0)
  end
end

--- print audio file info 
-- @tparam string path (from dust directory)
function Audio.file_info(path)
  -- dur, ch, rate
  --print("file_info: " .. path)
  return _norns.sound_file_inspect(path)
end


function Audio.add_params()
  params:add_group("LEVELS",9)
  params:add_control("output_level", "output", 
    cs.new(-math.huge,6,'db',0,norns.state.mix.output,"dB"))
  params:set_action("output_level",
    function(x) 
      audio.level_dac(util.dbamp(x))
      norns.state.mix.output = x
    end)
  params:set_save("output_level", false)
  params:add_control("input_level", "input",
    cs.new(-math.huge,6,'db',0,norns.state.mix.input,"dB"))
  params:set_action("input_level",
    function(x)
      audio.level_adc(util.dbamp(x))
      norns.state.mix.input = x
    end)
  params:set_save("input_level", false)
  params:add_control("monitor_level", "monitor",
    cs.new(-math.huge,6,'db',0,norns.state.mix.monitor,"dB"))
  params:set_action("monitor_level",
    function(x)
      audio.level_monitor(util.dbamp(x))
      norns.state.mix.monitor = x
    end)
  params:set_save("monitor_level", false)
  params:add_control("engine_level", "engine",
    cs.new(-math.huge,6,'db',0,norns.state.mix.engine,"dB"))
  params:set_action("engine_level",
    function(x)
      audio.level_eng(util.dbamp(x))
      norns.state.mix.engine = x
    end)
  params:set_save("engine_level", false)
  params:add_control("softcut_level", "softcut",
    cs.new(-math.huge,6,'db',0,norns.state.mix.cut,"dB"))
  params:set_action("softcut_level",
    function(x)
      audio.level_cut(util.dbamp(x))
      norns.state.mix.cut = x
    end)
  params:set_save("softcut_level", false)
  params:add_control("tape_level", "tape",
    cs.new(-math.huge,6,'db',0,norns.state.mix.tape,"dB"))
  params:set_action("tape_level",
    function(x)
      audio.level_tape(util.dbamp(x))
      norns.state.mix.tape = x
    end)
  params:set_save("tape_level", false)
  params:add_separator()
  params:add_option("monitor_mode", "monitor mode", {"STEREO", "MONO"},
    norns.state.mix.monitor_mode)
  params:set_action("monitor_mode",
    function(x)
      if x == 1 then audio.monitor_stereo()
      else audio.monitor_mono() end
      norns.state.mix.monitor_mode = x
    end)
  params:set_save("monitor_mode", false)
  params:add_number("headphone_gain", "headphone gain", 0, 63,
    norns.state.mix.headphone_gain)
  params:set_action("headphone_gain",
    function(x)
      audio.headphone_gain(x)
      norns.state.mix.headphone_gain = x
    end)
  params:set_save("headphone_gain", false)
  
  params:add_group("REVERB",11)
  params:add_option("reverb", "reverb", {"OFF", "ON"}, 
    norns.state.mix.aux)
  params:set_action("reverb",
  function(x)
    if x == 1 then audio.rev_off() else audio.rev_on() end
    norns.state.mix.aux = x
  end)
  params:set_save("reverb", false)

  --local cs_DB_LEVEL = cs.new(-math.huge,18,'db',0,0,"dB")
  --local cs_DB_LEVEL_MUTE = cs.new(-math.huge,18,'db',0,-math.huge,"dB")
  --local cs_DB_LEVEL_9DB = cs.new(-math.huge,18,'db',0,-9,"dB")

  params:add_control("rev_eng_input", "input engine",
    cs.new(-math.huge,18,'db',0,norns.state.mix.rev_eng_input,"dB"))
  params:set_action("rev_eng_input",
  function(x)
    audio.level_eng_rev(util.dbamp(x))
    norns.state.mix.rev_eng_input = x
  end)

  params:add_control("rev_cut_input", "input softcut", 
    cs.new(-math.huge,18,'db',0,norns.state.mix.rev_eng_input,"dB"))
  params:set_action("rev_cut_input",
  function(x)
    audio.level_cut_rev(util.dbamp(x))
    norns.state.mix.rev_cut_input = x
  end)

  params:add_control("rev_monitor_input", "input monitor",
    cs.new(-math.huge,18,'db',0,norns.state.mix.rev_monitor_input,"dB"))
  params:set_action("rev_monitor_input",
  function(x)
    audio.level_monitor_rev(util.dbamp(x))
    norns.state.mix.rev_monitor_input = x
  end)

  params:add_control("rev_tape_input", "input tape",
    cs.new(-math.huge,18,'db',0,norns.state.mix.rev_tape_input,"dB"))
  params:set_action("rev_tape_input",
  function(x)
    audio.level_tape_rev(util.dbamp(x))
    norns.state.mix.rev_tape_input = x
  end)

  params:add_control("rev_return_level", "return level",
    cs.new(-math.huge,18,'db',0,norns.state.mix.rev_return_level,"dB"))
  params:set_action("rev_return_level",
  function(x)
    audio.level_rev_dac(util.dbamp(x))
    norns.state.mix.rev_return_level = x
  end)

  params:add_control("rev_pre_delay", "pre delay",
    cs.new(20,100,'lin',0,norns.state.mix.rev_pre_delay,'ms'))
  params:set_action("rev_pre_delay",
  function(x)
    audio.rev_param("pre_del",x)
    norns.state.mix.rev_pre_delay = x
  end)

  params:add_control("rev_lf_fc", "lf fc",
    cs.new(50,1000,'exp',0, norns.state.mix.rev_lf_fc,'hz'))
  params:set_action("rev_lf_fc",
  function(x)
    audio.rev_param("lf_fc",x)
    norns.state.mix.rev_lf_fc = x
  end)

  params:add_control("rev_low_time", "low time",
    cs.new(0.1,16,'lin',0,norns.state.mix.rev_low_time,'s'))
  params:set_action("rev_low_time",
  function(x)
    audio.rev_param("low_rt60",x)
    norns.state.mix.rev_low_time = x
  end)

  params:add_control("rev_mid_time", "mid time",
    cs.new(0.1,16,'lin',0,norns.state.mix.rev_mid_time,'s'))
  params:set_action("rev_mid_time",
  function(x)
    audio.rev_param("mid_rt60",x)
    norns.state.mix.rev_mid_time = x
  end)

  params:add_control("rev_hf_damping", "hf damping", 
    cs.new(1500,20000,'exp',0,norns.state.mix.rev_hf_damping,'hz'))
  params:set_action("rev_hf_damping",
  function(x)
    audio.rev_param("hf_damp",x)
    norns.state.mix.rev_hf_damping = x
  end)

  --[[
  local cs_EQ_FREQ1 = cs.new(40,2500,'exp',0,315,'hz')
  params:add_control("rev_eq1_freq","rev eq1 freq", cs_EQ_FREQ1)
  params:set_action("rev_eq1_freq",
  function(x) audio.aux_param("eq1_freq",x) end)
  local cs_EQ_LVL = cs.new(-15,15,'lin',0,0,"dB")
  params:add_control("rev_eq1_level","rev eq1 level", cs_EQ_LVL)
  params:set_action("rev_eq1_level",
  function(x) audio.aux_param("eq1_level",x) end)

  local cs_EQ_FREQ2 = cs.new(160,10000,'exp',0,1500,'hz')
  params:add_control("rev_eq2_freq","rev eq2 freq", cs_EQ_FREQ2)
  params:set_action("rev_eq2_freq",
  function(x) audio.aux_param("eq2_freq",x) end)
  params:add_control("rev_eq2_level","rev eq2 level", cs_EQ_LVL)
  params:set_action("rev_eq2_level",
  function(x) audio.aux_param("eq2_level",x) end)

  params:add_control("rev_level","rev level", cs_DB_LEVEL)
  params:set_action("rev_level",
  function(x) audio.aux_param("level",x) end)
  --]]

  params:add_group("COMPRESSOR",8)
  params:add_option("compressor", "compressor", {"OFF", "ON"},
    norns.state.mix.ins)
  params:set_action("compressor",
  function(x)
    if x == 1 then
      audio.comp_off()
    else
      audio.comp_on()
    end
    norns.state.mix.ins = x
  end)
  params:set_save("compressor", false)

  params:add_control("comp_mix", "mix",
    cs.new(0,1,'lin',0,norns.state.mix.comp_mix,''))
  params:set_action("comp_mix",
  function(x)
    audio.comp_mix(x)
    norns.state.mix.comp_mix = x
  end)

  params:add_control("comp_ratio", "ratio",
    cs.new(1,20,'lin',0,norns.state.mix.comp_ratio,''))
  params:set_action("comp_ratio",
  function(x)
    audio.comp_param("ratio",x)
    norns.state.mix.comp_ratio = x
  end)

  params:add_control("comp_threshold", "threshold",
    cs.new(-100,10,'db',0,norns.state.mix.comp_threshold,'dB'))
    params:set_action("comp_threshold",
  function(x)
    audio.comp_param("threshold",x)
    norns.state.mix.comp_threshold = x
  end)

  params:add_control("comp_attack", "attack",
    cs.new(1,1000,'exp',0,norns.state.mix.comp_attack,'ms'))
  params:set_action("comp_attack",
  function(x)
    audio.comp_param("attack", x*0.001)
    norns.state.mix.comp_attack = x
  end)

  
  params:add_control("comp_release", "release",
    cs.new(1,1000,'exp',0,norns.state.mix.comp_release,'ms'))
  params:set_action("comp_release",
  function(x)
    audio.comp_param("release",x * 0.001)
    norns.state.mix.comp_release = x
  end)

  params:add_control("comp_pre_gain", "pre gain",
    cs.new(-20,60,'db',0,norns.state.mix.comp_pre_gain,'dB'))
  params:set_action("comp_pre_gain",
  function(x)
    audio.comp_param("gain_pre",x)
    norns.state.mix.comp_pre_gain = x
  end)

  params:add_control("comp_post_gain", "post gain",
    cs.new(-20,60,'db',0,norns.state.mix.comp_post_gain,'dB'))
  params:set_action("comp_post_gain",
  function(x)
    audio.comp_param("gain_post",x)
    norns.state.mix.comp_post_gain = x
  end)


  params:add_group("SOFTCUT",3)

  params:add_control("cut_input_adc", "input adc",
    cs.new(-math.huge,18,'db',0,norns.state.mix.cut_input_adc,"dB"))
  params:set_action("cut_input_adc",
    function(x)
      audio.level_adc_cut(util.dbamp(x))
      norns.state.mix.cut_input_adc = x
    end)

  params:add_control("cut_input_eng", "input engine",
    cs.new(-math.huge,18,'db',0,norns.state.mix.cut_input_eng,"dB"))
  params:set_action("cut_input_eng",
    function(x)
      audio.level_eng_cut(util.dbamp(x))
      norns.state.mix.cut_input_eng = x
    end)

  params:add_control("cut_input_tape", "input tape",
    cs.new(-math.huge,18,'db',0,norns.state.mix.cut_input_tape,"dB"))
  params:set_action("cut_input_tape",
    function(x)
      audio.level_tape_cut(util.dbamp(x))
      norns.state.mix.cut_input_tape = x
    end)

end

return Audio
