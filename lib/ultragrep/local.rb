require 'ultragrep/executor'

module Ultragrep
  class Local < Executor
    def bin_path
      File.join(File.dirname(__FILE__), '..', '..', 'src')
    end

    def lua
      File.join(File.dirname(__FILE__), '..', '..', 'lua', @config.lua)
    end

    def popen(command, core)
      IO.popen("#{command} | #{core}")
    end
  end
end
