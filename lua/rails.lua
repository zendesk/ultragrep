request = { ['offset'] = 0, ['data'] = {} }
strptime_format = "%Y-%m-%d %H:%M:%S"
blanks = 0

function process_line(line, offset)
  if line == "\n" then
    blanks = blanks + 1
  else
    if blanks >= 2 and request['data'] then
      -- insert the last request, reset
      request['data'] = table.concat(request['data'])
      ug_request.add(request['data'], request['ts'], request['offset'])

      request = { ['offset'] = offset, ['data'] = {} }
      blanks = 0 
    end

    if not request['ts'] then
      _, _, ts = string.find(line, "at (%d%d%d%d%-%d%d%-%d%d %d%d:%d%d:%d%d)")
      if ts then 
        request['ts'] =  ts 
      end
    end
    table.insert(request['data'], line)
  end
end

function on_eof()
  request['data'] = table.concat(request['data'])
  ug_request.add(request['data'], request['ts'], request['offset'])
end
