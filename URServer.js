// Universal Remote server example (CuriousTech PngMagic script)
// This creates a server on port 85
// The Reg.xxx values are in other scripts such as the thermostat and GDO

localPort = 85
fontHeight = 12
statTemp = 0
statRh = 0

sum = old_sum = 0
old_pos = 0

var gdoRh
gdoCar = 1
var gdoDoor
gdoState = 0 // default

Http.Close()
Http.Server(localPort)

Pm.SetTimer(1000)

function OnTimer()
{
	SendVars()
}

function OnCall( msg, method, path )
{
// Pm.Echo('Server ' + msg + ' ' + method + ' ' + path)

	switch(msg)
	{
		case 'BUTTON':
			break
		case 'HTTPREQ':

			if(path == '/ws')
			{
				old_sum = 0
				old_pos = 0
				Pm.Echo('URServer WS connected')
			}else
			{
				s = 'HTTP/1.1 200 OK\r\n\r\nURemote WebSocket Server\r\n' + Http.IP + '\r\n'
				Http.Send( s )
			}
			SendVars()
			break
		case 'HTTPDATA':
			Pm.Echo('URServer Data from ' + method + ' : ' + path)
			break
		case 'HTTPSENT':
			Pm.Echo('URServer Sent data' + method + ' ' + path)
			break
		case 'HTTPSTATUS':
			Pm.Echo('URServer HTTP Status ' + method + ' ' + path)
			break
		case 'WEBSOCKET':
			doWs(method)
			break
		case 'HTTPCLOSE':
			break
		default:
			Pm.Echo('URServer: default ' + msg)
			break
	}
}

function doWs(data)
{
	ws = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	Pm.Echo('URServer WS:'+data)

	switch(ws.cmd)
	{
		case 'media':
			Pm.Media('HOTKEY', ws.HOTKEY)
			break
		case 'volume':
			Pm.Volume = +ws.value
			break
		case 'pos':
			Media.Position = ws.value * Media.Duration / 100
			break
		case 'print':
			Pm.Echo(ws.text)
			break
		case 'STAT':
			switch(+ws.value)
			{
				case 0: // temp up
					break
				case 1: // temp down
					break
				case 2: // fan toggle
					Pm.HvacRemote('BUTTON', 'FANMODE')
					break
			}
			break
		case 'GDOCMD':
			Pm.Echo('GDO Cmd ' + ws.value)
			break
	}
}

function SendVars()
{
	if(Media.Position != old_pos)
	{
		old_pos = Media.Position
		pos = Math.floor(Media.Position * 100 / Media.Duration)
		s = '{'
		s +='"PC_MED_POS":' +pos
		s += '}'
		Http.WsSend(s)
	}

	sum = Pm.Volume + Reg.statTemp + Reg.statSetTemp + Reg.outTemp + Reg.gdoDoor + Reg.gdoCar + Reg.statFan
	if(sum == old_sum)
		return

	old_sum = sum

	s = '{'
	s +='"PC_REMOTE":0,'
	s +='"PC_VOLUME":' + Pm.Volume.toFixed() + ','
	s +='"STATTEMP":' + Reg.statTemp + ','
	s +='"STATSETTEMP":' + Reg.statSetTemp + ','
	s +='"STATFAN":' + Reg.statFan + ','
	s +='"OUTTEMP":' + Reg.outTemp*10 + ','
	s +='"GDODOOR":' + Reg.gdoDoor + ','
	s +='"GDOCAR":' + Reg.gdoCar
	s += '}'
	Http.WsSend(s)
}
