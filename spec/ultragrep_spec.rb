# encoding: utf-8
require "tmpdir"
require "yaml"
require "ultragrep"
require "bundler/setup"
begin ;require "debugger" ; rescue LoadError => e; end
begin ;require "byebug" ; rescue LoadError => e; end

ENV['TZ'] = 'UTC'

describe Ultragrep do
  def run(command, options={})
    result = `#{command} 2>&1`
    message = (options[:fail] ? "SUCCESS BUT SHOULD FAIL" : "FAIL")
    raise "[#{message}] #{result} [#{command}]" if $?.success? == !!options[:fail]
    result
  end

  def ultragrep(args, options={})
    run "#{Bundler.root}/bin/ultragrep #{args}", options
  end

  def write(file, content)
    FileUtils.mkdir_p(File.dirname(file))
    File.write(file, content)
  end

  def write_config
    luadir = File.expand_path(File.dirname(__FILE__)) + "/../lua"
    config = {"types" => { "app" => { "glob" => "foo/*/*", "lua" => luadir + "/rails.lua"},
                           "work" => { "glob" => "work/*/*", "format" => "work" } },
              "default_type" => "app" }
    File.write(".ultragrep.yml", config.to_yaml)
  end

  def time_format
    "%Y-%m-%d %H:%M:%S"
  end

  def fake_ultragrep_logs
    write "foo/host.1/a.log-#{date}", "Processing xxx at #{time_at}\n"
    write "bar/host.1/a.log-#{date}", "Processing yyy at #{time_at}\n"
    write "work/host.1/a.log-#{date}", %{{"time":"#{time_at}","session":"f6add2:a51f27"}\n}
    write "foo/host.1/b.log-#{date}", <<-EOL
Processing -60 at 2012-01-01 00:00:00\n\n
Processing -50 at 2012-01-01 00:01:00\n\n
Processing -44 at 2012-01-01 00:01:12\n\n
Processing -40 at 2012-01-01 00:03:52\n\n
Processing -29 at 2012-01-01 00:03:53\n\n
Processing -10 at 2012-01-01 01:00:00\n\n
    EOL
  end

  def test_time_is_found(success, ago, command, options={})
    time = (Time.now - ago).strftime(time_format)
    write "foo/host.1/a.log-#{date}", "Processing xxx at #{time}\n"
    output = ultragrep("at #{command}", options)
    if success
      expect(output).to include "Processing"
    else
      expect(output.strip).to eq("")
    end
  end

  def test_is_found(success, command)
    test_time_is_found(success, 0, command)
  end

  def date(delta=0)
    (Time.now - (delta * day)).strftime("%Y%m%d")
  end

  def time_at(delta=0)
    (Time.now - delta).strftime(time_format)
  end

  let(:day) { 24 * hour }
  let(:hour) { 60 * 60 }

  around do |example|
    Dir.mktmpdir do |dir|
      Dir.chdir(dir, &example)
    end
  end

  before { write_config }

=begin
  describe "Json Logs" do
    let(:time) { time_at(0) }
    it "should work for json logs" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"request_id\":\"abc\",\"time\": \"#{time}\"}"

      output =  ultragrep("-l json -s '2012-01-01' -e '#{time}' abc")
      output.strip.should == "# json/host.1/b.log-#{date}.json\n{\n    \"account_id\": 1,\n    \"request_id\": \"abc\",\n    \"time\": \"#{time}\"\n}\n--------------------------------------------------------------------------------"
    end

    it "should parse Integer key-value" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"request_id\":\"abc\",\"time\": \"#{time}\"}"
      file = File.open("json/host.1/b.log-#{date}.json", "rb")

      output =  ultragrep("-l json -k account_id=1 -s '2012-01-01' -e '#{time}' a")
      output.strip.should == "# json/host.1/b.log-#{date}.json\n{\n    \"account_id\": 1,\n    \"request_id\": \"abc\",\n    \"time\": \"#{time}\"\n}\n--------------------------------------------------------------------------------"
    end

    it "should parse Bool key-value" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"request_id\":\"false\",\"time\": \"#{time}\"}"

      output =  ultragrep("-l json -k request_id=false -s '2012-01-01' -e '#{time}' 1")
      output.strip.should == "# json/host.1/b.log-#{date}.json\n{\n    \"account_id\": 1,\n    \"request_id\": \"false\",\n    \"time\": \"#{time}\"\n}\n--------------------------------------------------------------------------------"
      end

    it "should parse Not Pass key-value" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"\":\"abc\",\"time\": \"#{time}\"}"

      output =  ultragrep("-l json -k request_id=abc -s '2012-01-01' -e '#{time}' 1")
      output.strip.should == ""
    end

    it "should parse multiple key-value" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1, \"request_id\":\"awesome\",\"time\": \"#{time}\", \"firstname\": \"test\", \"lastname\": \"name\"}"

      output =  ultragrep("-l json -k request_id=awesome -k firstname=test -s '2012-01-01' -e '#{time}' name")
      output.strip.should == "# json/host.1/b.log-#{date}.json\n{\n    \"account_id\": 1,\n    \"request_id\": \"awesome\",\n    \"time\": \"#{time}\",\n    \"firstname\": \"test\",\n    \"lastname\": \"name\"\n}\n--------------------------------------------------------------------------------"
    end

    it "should fail for the  empty unicode character" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"request_id\":\"abc\"u0000\",\"time\": \"#{time}\"}"

      output =  ultragrep("-l json -k request_id=abc -s '2012-01-01' -e '#{time}' 1")
      output.strip.should == ""
    end

    it "should fail time is not in range" do
      date = date()
      write "json/host.1/b.log-#{date}.json", "{\"account_id\":1,\"request_id\":\"abc\",\"time\": \"2011-01-01\"}"

      output =  ultragrep("-l json -k request_id=abc -s '#{time}' -e '#{time}' 1")
      output.strip.should == ""
    end
  end
=end

  describe "CLI" do
    describe "basics" do
      it "shows --help" do
        expect(ultragrep("--help")).to include "Usage: "
      end

      it "should show help when no regex is given" do
        expect(ultragrep("", :fail => true)).to include "Usage: "
      end

      it "warns about missing config" do
        File.unlink(".ultragrep.yml")
        result = ultragrep "aaa", :fail => true
        expect(result).to include "Please configure ultragrep.yml"
      end

      it "shows --version" do
        expect(ultragrep("--version")).to match(/Ultragrep version \d+\.\d+\.\d+/)
      end

      it "warns about time missuse" do
        expect(ultragrep("2013-01-01 12:12:12 --version")).to match /Put time inside quotes like this '2013-01-01 12:12:12'/
      end
    end

    describe "grepping" do
      let(:time) { time_at(0) }
      it "greps through 1 file" do
        date = date()
        write "foo/host.1/a.log-#{date}", "Processing xxx at #{time}\n"
        output =  ultragrep("at").strip
        expect(output).to include "# foo/host.1/a.log-#{date}\n"
        expect(output).to include "xxx"
      end

      it "reads from config file" do
        run "mv .ultragrep.yml custom-location.yml"
        write "foo/host.1/a.log-#{date}", "Processing xxx at #{time}\n"
        output =  ultragrep("at --config custom-location.yml").strip
        expect(output).to include "xxx"
      end

=begin  -- should introduce work.lua-ish thing to test
      it "use different location via --type" do
        fake_ultragrep_logs
        output = ultragrep("f6add2 --type work")
        output.should include "f6add2"
        output.should_not include "Processing"
      end
=end

      context "default range" do
        let(:time_since_start_of_day) { Time.now.to_i % day }

        before do
          pending "too close to day border, tests would fail" if time_since_start_of_day < 1.5 * hour
        end

        context "start" do
          it "ignores older then start of day" do
            test_time_is_found(true, time_since_start_of_day - hour, "")
          end

          it "finds after start of day" do
            test_time_is_found(false, time_since_start_of_day + hour, "")
          end
        end

        context "end" do
          it "ignores after current time" do
            test_time_is_found(false, -hour, "")
          end

          it "find before current time" do
            test_time_is_found(true, hour, "")
          end
        end
      end

      context "--start" do
        let(:time) { Time.now - (2 * hour) }

        it "blows up with incorrect time format" do
          ultragrep("--start asdadasdsd", :fail => true)
        end

        context "with nice format" do
          it "ignores things before start" do
            test_time_is_found(false, 3 * hour, "--start '#{time.utc.strftime("%Y-%m-%d %H:%M:%S")}'")
          end

          it "finds things after start" do
            test_time_is_found(true, hour, "--start '#{time.utc.strftime("%Y-%m-%d %H:%M:%S")}'")
          end
        end

        context "with integers" do
          it "ignores things before start" do
            test_time_is_found(false, 3 * hour, "--start #{time.to_i}")
          end

          it "finds things after start" do
            test_time_is_found(true, hour, "--start #{time.to_i}")
          end
        end
      end

      context "--end" do
        let(:time) { Time.now }
        let(:time_since_start_of_day) { Time.now.to_i % day }

        before do
          pending "too close to day border, tests would fail" if time_since_start_of_day < 2.5 * hour
        end

        it "ignores things after end" do
          test_time_is_found(false, hour, "--end '#{(time.utc - hour * 2).strftime("%Y-%m-%d %H:%M:%S")}'")
        end

        it "finds things before end" do
          test_time_is_found(true, hour, "--end '#{time.utc.strftime("%Y-%m-%d %H:%M:%S")}'")
        end
      end

      context "--around" do
        let(:time) { Time.now - (1 * hour) }
        let(:time_since_start_of_day) { Time.now.to_i % day }

        before do
          pending "too close to day border, tests would fail" if time_since_start_of_day < 1.5 * hour
        end

        it "finds things around that time" do
          test_time_is_found(true, hour, "--around '#{time.utc.strftime("%Y-%m-%d %H:%M:%S")}'")
        end
      end

      context "--host" do
        before do
          # do not blow up because of missing files
          write "foo/host.2/a.log-#{date}", "UNMATCHED"
          write "foo/host.3/a.log-#{date}", "UNMATCHED"
        end

        context "single" do
          it "finds wanted host" do
            test_is_found(true, "--host host.1")
          end

          it "ignores unwanted host" do
            test_is_found(false, "--host host.2")
          end
        end

        context "multiple" do
          it "find wanted host" do
            test_is_found(true, "--host host.2 --host host.1 --host host.2")
          end

          it "ignores unwanted host" do
            test_is_found(false, "--host host.2 --host host.3")
          end
        end
      end

      context "--not" do
        it "inverts a given regular expression's assertion" do
          date = date()
          write "foo/host.1/a.log-#{date}", "Processing xxx/yyy at #{time}\n\n\nProcessing xxx/zzz at #{time}\n\n\n"
          output = ultragrep("xxx --not yyy")
          expect(output).to_not include "xxx/yyy"
          expect(output).to include "xxx/zzz"
        end
      end

      context "--progress" do
        before do
          write "foo/host.1/a.log-#{date}", "UNMATCHED"
        end

        it "shows file list" do
          result = ultragrep("xxx --progress")
          expect(result).to include "searching for regexps: xxx from "
          expect(result).to include "searching foo/host.1/a.log-#{date}"
        end

        it "does not show file list without" do
          result = ultragrep("xxx")
          expect(result).to_not include "searching for regexps: xxx from "
          expect(result).to_not include "searching foo/host.1/a.log-#{date}"
        end
      end

      describe "--perf" do
        it "shows performance info" do
          write "foo/host.1/a.log-#{date}", "Processing xxx at #{time_at}\nCompleted in 100ms\n\n\nProcessing xxx at #{time_at}\nCompleted in 200ms\n\n\nProcessing xxx at #{time_at}\nCompleted in 100ms\n"
          output = ultragrep("at --perf")
          output.gsub!(/\d{6,}/, "TIME")
          expect(output.strip).to eq "TIME\txxx\t100\nTIME\txxx\t100\nTIME\txxx\t200"
        end
      end

      describe "--day" do
        it "picks everything from entire day" do
          write "foo/host.1/a.log-20130201", "Processing xxx at 2013-02-01 12:00:00\n"
          write "foo/host.1/a.log-20130202", "Processing xxx at 2013-02-02 12:00:00\n"
          write "foo/host.1/a.log-20130203", "Processing xxx at 2013-02-03 12:00:00\n"
          output = ultragrep("at --day '2013-02-02'")
          expect(output.scan(/\d+-\d+-\d+ \d+:\d+:\d+/)).to eq ["2013-02-02 12:00:00"]
        end

        it "picks everything from 24 hour period" do
          write "foo/host.1/a.log-20130202", "Processing xxx at 2013-02-02 11:00:00\n\n\nProcessing xxx at 2013-02-02 13:00:00\n"
          write "foo/host.1/a.log-20130203", "Processing xxx at 2013-02-03 11:00:00\n\n\nProcessing xxx at 2013-02-03 23:00:00\n"
          output = ultragrep("at --day '2013-02-02 12:00:00'")
          expect(output.scan(/\d+-\d+-\d+ \d+:\d+:\d+/)).to eq ["2013-02-02 13:00:00", "2013-02-03 11:00:00"]
        end
      end

      describe "--daysback" do
        it "picks everything in the given range" do
          write "foo/host.1/a.log-#{date}", "Processing xxx at #{time_at}\n"
          write "foo/host.1/a.log-#{date(1)}", "Processing xxx at #{time_at((1 * day) + 10)}\n"
          write "foo/host.1/a.log-#{date(2)}", "Processing xxx at #{time_at((2 * day) + 10)}\n"
          write "foo/host.1/a.log-#{date(3)}", "Processing xxx at #{time_at((3 * day) + 10)}\n"
          output = ultragrep("at --daysback 2")
          expect(output.scan(/\d+-\d+-\d+/).map{|x|x.gsub("-", "")}).to eq [date(1), date]
        end
      end
    end

    describe "building indexes" do
      let(:date) { Time.now.strftime("%Y%m%d") }
      let(:log_file) { "foo/host.1/b.log-#{date}" }
      let(:index_file) { "foo/host.1/.b.log-#{date}.idx" }

      describe "ug_build_index" do
        before do
          fake_ultragrep_logs
          system "rm -f #{index_file}"
          run "#{Bundler.root}/src/ug_build_index #{File.dirname(__FILE__) + "/../lua/rails.lua"} #{log_file}"
        end

        it "should drop a log file to disk" do
            expect(File.exist?(index_file)).to be true
        end

        it "should have time to offset indexes" do
          dump_index = File.dirname(__FILE__) + "/dump_index.rb"
          index_dumped = `ruby #{dump_index} #{index_file}`
          expect(index_dumped).to eq "1325376000 0\n1325376060 40\n1325376070 80\n1325376230 120\n1325379600 200\n"
        end

        describe "with a gzipped file" do
          before do
            system "gzip #{log_file}"
            system "rm -f #{index_file}.gz"
            run "#{Bundler.root}/src/ug_build_index  #{File.dirname(__FILE__) + "/../lua/rails.lua"} #{log_file}.gz"
          end

          it "should not crash" do
            dump_index = File.dirname(__FILE__) + "/dump_index.rb"
            index_dumped = `ruby #{dump_index} foo/host.1/.b.log-#{date}.gz.idx`
            expect(index_dumped).to eq "1325376000 0\n1325376060 40\n1325376070 80\n1325376230 120\n1325379600 200\n"

          end
        end

      end
    end
  end

  describe ".parse_time" do
    let(:zone_offset) { Time.zone_offset(Time.now.zone) }

    it "parses int" do
      expected = Time.now.to_i
      expect(Ultragrep.send(:parse_time, expected.to_s).to_i).to eq expected
    end

    it "parses string" do
      expect(Ultragrep.send(:parse_time, "2013-01-01").to_i).to eq Time.new(2013,01,01).to_i
    end

    it "parses weird string" do
      expect(Ultragrep.send(:parse_time, "20130101").to_i).to eq Time.new(2013,01,01).to_i
    end

    it "blows up on invalid time" do
      expect{
        Ultragrep.send(:parse_time, "asdasdas")
      }.to raise_exception ArgumentError
    end

    it "parses string with time" do
      expect(Ultragrep.send(:parse_time, "2013-01-01 12:23:34").to_i).to eq Time.new(2013,01,01,12,23,34).to_i
    end
  end

  describe ".quote_shell_words" do
    it "quotes" do
      expect(Ultragrep.send(:quote_shell_words, ["abc", "def"])).to eq "'abc' 'def'"
    end

    it "quotes single quotes" do
      expect(Ultragrep.send(:quote_shell_words, ["a'bc", "def"])).to eq "'a.bc' 'def'"
    end
  end

  describe ".encode_utf8!" do
    it "removes invalid utf8" do
      line = "€foo\xA0bar"
      expect(Ultragrep.send(:encode_utf8!, line)).to eq "€foobar"
    end
  end

  describe Ultragrep::LogCollector do
    describe ".filter_and_group_files" do
      it "returns everything when not filtering by host" do
        t = Time.now.to_i
        c = Ultragrep::LogCollector.new(nil, :range_start => t - day, :range_end => t + day)
        result = c.filter_and_group_files(["a/b/c-#{date}"])
        expect(result).to eq [["a/b/c-#{date}"]]
      end

      it "excludes days before and after and groups by date" do
        t = Time.parse("2013-01-10 12:00:00 UTC").to_i
        c = Ultragrep::LogCollector.new(nil, :range_start => t, :range_end => t + day)
        result = c.filter_and_group_files(["a/b/c-20130109", "a/b/c-20130110", "a/b/d-20130110", "a/b/c-20130111", "a/b/c-20130112"])
        expect(result).to eq [["a/b/c-20130110", "a/b/d-20130110"], ["a/b/c-20130111"]]
      end

      it "does not exclude when range is inside" do
        t = Time.parse("2013-01-10 12:00:00 UTC").to_i
        c = Ultragrep::LogCollector.new(nil, :range_start => t, :range_end => t)
        result = c.filter_and_group_files(["a/b/c-20130109", "a/b/c-20130110", "a/b/d-20130110", "a/b/c-20130111", "a/b/c-20130112"])
        expect(result).to eq [["a/b/c-20130110", "a/b/d-20130110"]]
      end

      it "excludes hosts" do
        t = Time.parse("2013-01-10 12:00:00 UTC").to_i
        c = Ultragrep::LogCollector.new(nil, :range_start => t - day, :range_end => t, :host_filter => ["b"])
        result = c.filter_and_group_files(["a/a/c-20130110", "a/b/c-20130110", "a/c/c-20130110"])
        expect(result).to eq [["a/b/c-20130110"]]
      end

      it "does not crash on files that don't match the regexp" do
        t = Time.parse("2013-01-10 12:00:00 UTC").to_i
        c = Ultragrep::LogCollector.new(nil, :range_start => t - day, :range_end => t)
        result = c.filter_and_group_files(["a/a/c", "a/b/c-20130110"])
        expect(result).to eq [["a/b/c-20130110"]]
      end
    end
  end

  describe "ultragrep_build_indexes" do
    before do
      fake_ultragrep_logs
    end

    it "succeeds" do
      run "#{Bundler.root}/bin/ultragrep_build_indexes -t app"
    end

    it "builds indexes" do
      run "#{Bundler.root}/bin/ultragrep_build_indexes -t app"
      expect(File.exists?("foo/host.1/.a.log-#{date}.idx")).to eq true
    end
  end
end

