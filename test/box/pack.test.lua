-- Test box.pack()
box.pack()
box.pack(1)
box.pack('abc')
box.pack('a', ' - hello')
box.pack('Aa', ' - hello', ' world')
string.sub(box.pack('p', 1684234849), 2)
box.pack('p', 'this string is 45 characters long 1234567890 ')
box.pack('s', 0x4d)
box.pack('ssss', 25940, 29811, 28448, 11883)
box.pack('SSSS', 25940, 29811, 28448, 11883)
box.pack('SSSSSSSS', 28493, 29550, 27680, 27497, 29541, 20512, 29285, 8556)
box.pack('bsil', 84, 29541, 1802444916, 2338318684567380014ULL)
box.unpack('b', 'T')
box.unpack('s', 'Te')
box.unpack('i', 'Test')
box.unpack('l', 'Test ok.')
box.unpack('bsil', box.pack('bsil', 255, 65535, 4294967295, tonumber64('18446744073709551615')))
box.unpack('ppp', box.pack('ppp', 'one', 'two', 'three'))
num, str, num64 = box.unpack('ppp', box.pack('ppp', 666, 'string', tonumber64('666666666666666')))
num, str, num64
box.unpack('=p', box.pack('=p', 1, '666'))
box.unpack('','')
box.unpack('ii', box.pack('i', 1))
box.unpack('i', box.pack('ii', 1, 1))
box.unpack('+p', box.pack('=p', 1, '666'))
box.unpack('p', box.pack('p', 'this this this this'))
