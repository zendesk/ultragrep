require 'open3'
require 'ultragrep/executor'

module Ultragrep
  class Remote < Executor
    def initialize(options, config)
      super(options, config)
      @hosts = config.remote_hosts
    end

    def bin_path
      "~/.ultragrep_remote"
    end

    def lua
      "~/.ultragrep_remote/#{@config.lua}"
    end

    def popen(host, cat_cmd, filter)
      cmd = ["ssh", host, "#{cat_cmd} | #{filter}"]

      if debug?
        debug_cmd = cmd.dup
        debug_cmd[2] = '"' + debug_cmd[2] + '"'
        $stderr.puts(debug_cmd.join(' '))
      end

      IO.popen(cmd)
    end

    def get_children_pipes(regexps)
      files = collect_files
      pipes = []
      files.each do |host, files|
        if @options[:tail]
          remote_cat = ["echo @@FILE:#{files.join}", ";", 'tail', '-f'] + files
        else
          remote_cat = [ug_cat, @options[:range_start], "~/.ultragrep_remote/indexes", lua] + files
        end
        remote_guts = [ug_guts, "-l", lua, "-s", @options[:range_start], "-e", @options[:range_end], regexps]

        pipes << [popen(host, remote_cat.join(' '), remote_guts.join(' ')), host + ":"]
      end
      pipes
    end

    def setup!
      @hosts.each do |host|
        $stderr.puts("setting up state on '#{host}'")
        system_dbg("ssh", host, "mkdir -p ~/.ultragrep_remote/indexes") || raise("Couldn't make remote dir on #{host}!")

        src_files = `git ls-files -- src`.split(/\s+/)

        command = ["scp"]
        command += src_files
        command << host + ":.ultragrep_remote/"

        system_dbg(*command) || raise("Couldn't scp source files to #{host}!")

        system_dbg("ssh", host, "cd .ultragrep_remote && make") || raise("Couldn't build source on #{host}!")

        system_dbg("scp", @config.local_lua_path, host + ":.ultragrep_remote/") || raise("Coudln't scp #{@config.local_lua_path} to #{host}")
      end
    end

    def collect_files
      files = {}
      glob = @config.log_path_glob

      @hosts.each do |host|
        host_files = []
        if @options[:tail]
          host_files << @config.primary_log
        else
          glob.each do |g|
            ssh_files = syscall("ssh", host, "ls -1tc #{g} | tac").split("\n")
            ssh_files.each do |f|
              host_files << f
            end
          end
        end
        files[host] ||= []
        files[host] += host_files
      end
      files
    end

    def system_dbg(*args)
      $stderr.puts(args.join(' ')) if debug?
      system(*args)
    end

    def syscall(*cmd)
      $stderr.puts(cmd.join(' ')) if debug?
      stdout, stderr, status = Open3.capture3(*cmd)
      status.success? && stdout.slice!(0..-(1 + $/.size)) # strip trailing eol
    end

  end
end
