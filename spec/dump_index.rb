#!/usr/bin/env ruby

file = ARGV[0]
File.open(file, "r") do |f|
  begin
    while string = f.read(16)
      time, offset = string.unpack("QQ")
      puts "#{time} #{offset}"
    end
  rescue
    puts $!
  end
end
