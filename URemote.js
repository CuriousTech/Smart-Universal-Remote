// Universal Remote stream listener (CuriousTech PngMagic script)
// Used for now instead of wasting space with a client on the remote

dbIP = '192.168.31.81' // set to remotel IP (port 80)
Url = 'ws://' + dbIP + '/ws'
if(!Http.Connected)
	Http.Connect( 'event', Url )  // Start the event stream

Pm.SetTimer(1000)
heartbeat = 0
sum = old_sum = 0

// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'HTTPCONNECTED':
			Pm.Echo('UR connected' )
			SendVars()
			break
		case 'HTTPDATA':
			heartbeat = new Date()
			if(data.length <= 2) break // keep-alive heartbeat
//	Pm.Echo('UR  ' + data)
			procLine( data )
			break
		case 'HTTPSTATUS':
			if(+data != 12018)
				Pm.Echo('UR status ' + data)
			break
		case 'HTTPCLOSE':
			if(heartbeat)
				Pm.Echo('UR stream closed ')
			heartbeat = 0
			break
		case 'TEMP':
			break
		default:
			Pm.Echo('UR def ' + msg + ' ' + event + ' ' + data)
			break
	}
}

// Update remote with displayed vars and states
function SendVars()
{
	sum = Pm.Volume + Reg.statTemp + Reg.statSetTemp + Reg.outTemp + Reg.gdoDoor + Reg.gdoCar + Reg.statFan
	if(sum == old_sum)
		return

	old_sum = sum

	s = '{'
	s +='"PC_REMOTE":0,'
	s +='"PC_VOLUME":' + Pm.Volume + ','
	s +='"STATTEMP":' + Reg.statTemp + ','
	s +='"STATSETTEMP":' + Reg.statSetTemp + ','
	s +='"STATFAN":' + Reg.statFan + ','
	s +='"OUTTEMP":' + Reg.outTemp*10 + ','
	s +='"GDODOOR":' + Reg.gdoDoor + ','
	s +='"GDOCAR":' + Reg.gdoCar + ','
	s += '}'
	Http.Send(s)
}

function procLine(data)
{
	if(data.length < 2) return
//Pm.Echo(data)
	data = data.replace(/\n|\r/g, "")
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	switch(Json.cmd)
	{
		case 'state':
			break
		case 'media':
			Pm.Media('HOTKEY', Json.HOTKEY)
			break
		case 'volume':
			Pm.Volume = +Json.value
			break
		case 'print':
			Pm.Echo(Json.text)
			break
		case 'STAT':
			switch(+Json.value)
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
			Pm.Echo('GDO Cmd ' + Json.value)
			break
		case 'setup':
		case 'send':
		case 'recieve':
			break
		default:
			Pm.Echo('UR def cmd: ' + Json.cmd)
			break
	}
}

function OnTimer()
{
	if(Http.Connected)
		SendVars()
	time = (new Date()).valueOf()
	if(time - heartbeat > 90*1000)
	{
		if(!Http.Connected)
			Http.Connect( 'event', Url )  // Start the event stream
	}
}
