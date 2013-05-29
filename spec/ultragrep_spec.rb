require "tmpdir"
require "yaml"

describe "Ultragrep" do
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

  def fake_ultragrep_config
    File.write("config.yml", {"types" => {"app" => "foo/*/*", "work" => "work/*/*"} }.to_yaml)
    write "foo/host.1/a.log-#{date}", "Processing xxx at #{time}\n"
    write "bar/host.1/a.log-#{date}", "Processing yyy at #{time}\n"
    write "work/host.1/a.log-#{date}", %{{"time":"#{time}","session":"f6add2:a51f27"}\n}
  end

  describe "basics" do
    it "shows --help" do
      ultragrep("--help").should include "Usage: "
      ultragrep("-h").should include "Usage: "
      ultragrep("").should include "Usage: "
    end

    it "shows --version"
  end

  describe "grepping" do
    around do |example|
      Dir.mktmpdir do |dir|
        Dir.chdir(dir, &example)
      end
    end

    let(:date) { Time.now.strftime("%Y%m%d") }
    let(:time) { Time.now.strftime("%Y-%m-%d %H:%M:%S") }

    it "greps through 1 file" do
      write "logs/host.1/a.log-#{date}", "Processing xxx at #{time}\n"
      output =  ultragrep("at")
      output.strip.should == "# logs/host.1/a.log-#{date}\nProcessing xxx at #{time}\n--------------------------------------"
    end

    it "reads logfile locations from config" do
      fake_ultragrep_config
      output =  ultragrep("at --config config.yml")
      output.should include "xxx"
      output.should_not include "yyy"
    end

    it "use different location via --type" do
      fake_ultragrep_config
      output =  ultragrep("f6add2 --type work --config config.yml")
      output.should include "f6add2"
      output.should_not include "Processing"
    end
  end
end
