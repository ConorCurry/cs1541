function vtranslate(vlen, num16, v)::Int
	if v < num16
		return vlen * v * 2
	else
		return vlen * (v + num16)
	end
end

function test(vlen, num16, bytes)
	return [vtranslate(vlen, num16, v) for v in 0:((bytes/vlen) - num16)]
end

@show test(4, 4, 128)
@show test(2, 4, 128)
@show test(2, 2, 128)
@show test(4, 16, 128)
@show test(4, 8, 128)
@show test(4, 0, 128)
