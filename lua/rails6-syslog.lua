requests = { }
strptime_format = "%Y-%m-%dT%H:%M:%S"

function dump(t, indent, done)
    done = done or {}
    indent = indent or 0

    done[t] = true

    for key, value in pairs(t) do
        print(string.rep("\t", indent))

        if (type(value) == "table" and not done[value]) then
            done[value] = true
            print(key, ":\n")

            dump(value, indent + 2, done)
            done[value] = nil
        else
            print(key, "\t=\t", value, "\n")
        end
    end
end

function process_line(line, offset)
  _, _, req_id = string.find(line, "(%w%w%w%w%w%w%w%w%-%w%w%w%w%-%w%w%w%w%-%w%w%w%w%-%w%w%w%w%w%w%w%w%w%w%w%w)")
  if not req_id then
     return
  end

  if not requests[req_id] then
    _, _, ts = string.find(line, "(%d%d%d%d%-%d%d%-%d%dT%d%d:%d%d:%d%d)")

    requests[req_id] = { ['offset'] = offset, ['data'] = {}, ['ts'] = ts }
  end
  table.insert(requests[req_id]['data'], line)

  if string.find(line, "Completed") then
    requests[req_id]['data'] = table.concat(requests[req_id]['data'])
    ug_request.add(requests[req_id]['data'], requests[req_id]['ts'], requests[req_id]['offset'])
    requests[req_id] = nil
  end
end

function on_eof()
end
