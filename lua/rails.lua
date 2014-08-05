request = { ['offset'] = 0 }
strptime_format = "%Y-%m-%d %H:%M:%S"

function process_line(line, offset)
  if line == "\n" then
    blanks = blanks or 0
    blanks = blanks + 1

    if blanks >= 2 and request['data'] then
      -- insert the last request
      request['data'] = table.concat(request['data'])
      ug_request.add(request['data'], request['ts'], request['offset'])

      request = { ['offset'] = offset }
      blanks = 0 
    end
  else
    if not request['ts'] then
      _, _, ts = string.find(line, "at (%d%d%d%d%-%d%d%-%d%d %d%d:%d%d:%d%d)")
      if ts then 
        request['ts'] =  ts 
      end
    end
    request['data'] = request['data'] or {}
    table.insert(request['data'], line)
  end
end
