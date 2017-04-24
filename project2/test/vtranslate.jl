function vtranslate(vlen, num16, v)::Int
	if v < num16
		return vlen * v * 2
	else
		return vlen * (v + num16)
	end
end

function test(vlen, num16, bytes)
	return [bin(vtranslate(vlen, num16, v)) for v in 0:((bytes/vlen) - num16)]
end

@show vtranslate(4, 4, 31)
@show (vtranslate(4, 4, 27))

@show test(4, 4, 63)
@show test(2, 4, 63)
@show test(2, 2, 63)
@show test(4, 16, 63)
@show test(4, 8, 63)
@show test(4, 0, 63)
