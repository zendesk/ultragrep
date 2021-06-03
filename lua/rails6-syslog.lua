requests = { }

-- this global must be set, it tells ultragrep how to interpret the timestamp.
strptime_format = "%Y-%m-%dT%H:%M:%S"

function process_line(line, offset)
  -- [69aaccc2-2af4-4f3b-ab70-34817982c187]   Rendered application/_head.html.erb
  _, _, req_id = string.find(line, "(%w%w%w%w%w%w%w%w%-%w%w%w%w%-%w%w%w%w%-%w%w%w%w%-%w%w%w%w%w%w%w%w%w%w%w%w)")
  if not req_id then
     return
  end

  if not requests[req_id] then
    -- first line of request.  get the timestamp from here:
    -- [2021-06-03T00:10:39.768527 #1]  INFO -- : [73657dd5-47c8-4e6a-b109-234cb1379da3]
    -- and put it in the requests table
    _, _, ts = string.find(line, "(%d%d%d%d%-%d%d%-%d%dT%d%d:%d%d:%d%d)")

    requests[req_id] = { ['offset'] = offset, ['data'] = {}, ['ts'] = ts }
  end

  -- I'm running logs through syslog so i skip some preamble:
  -- Jun  3 00:10:39 p1-production mainstage-web[750]: I, [2021-
  --                                                      ^^^^^
  --                                                      skip to here
  first_bracket, _ = string.find(line, ", %[")
  if first_bracket then
    table.insert(requests[req_id]['data'], string.sub(line, first_bracket + 2))
  end

  -- Completed 200 OK in 22ms, ends the request.
  -- We could be smarter about this but ok for now.
  if string.find(line, "Completed") then
    requests[req_id]['data'] = table.concat(requests[req_id]['data'])

    -- this line calls back into ultragrep
    ug_request.add(requests[req_id]['data'], requests[req_id]['ts'], requests[req_id]['offset'])
    requests[req_id] = nil
  end
end

function on_eof()
end
