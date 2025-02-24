request = function()
   wrk.method = "POST"
   wrk.headers["Content-Type"] = "application/json"
   return wrk.format("POST", "/data", nil, '{"message":"test"}')
end
