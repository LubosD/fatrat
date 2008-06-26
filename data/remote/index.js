function newwin(url)
{
	window.open(url, 'popup', 'width=660,height=500,toolbar=0');
	return false;
}

function checkTransfer(checkbox)
{
	checked = checkbox.checked;
	tr = document.getElementById(checkbox.id+'_tr');
	if(checked)
		tr.setAttribute('style', 'background-color: #265381; color: white');
	else
		tr.setAttribute('style', '');
	
	tds = tr.getElementsByTagName('a');
	
	for(i=0;i<tds.length;i++)
	{
		if(checked)
			tds[i].style.color = 'white';
		else
			tds[i].style.color = 'black';
	}
	if(window.getSelection)
		window.getSelection().removeAllRanges();
}

function clickedTransfer(event,ix)
{
	tname = event.target.tagName.toLowerCase();
	if(tname != 'td' && tname != 'tr')
		return;
	
	if(!event.ctrlKey)
	{
		for(var i=0;;i++)
		{
			checkbox = document.getElementById('transfer'+i);
			if(!checkbox)
				break;
			
			if(checkbox.checked)
			{
				checkbox.checked = false;
				checkTransfer(checkbox);
			}
		}
	}
	
	checkbox = document.getElementById('transfer'+ix);
	checkbox.checked = !checkbox.checked;
	checkTransfer(checkbox);
}

function checkAll(check)
{
	for(var i=0;;i++)
	{
		id = 'transfer'+i;
		obj = document.getElementById(id);
		if(!obj)
			break;
		obj.checked = check;
		checkTransfer(obj);
	}
}

function readCookie(name)
{
	var items = document.cookie.split(';');
	var name_is = name + "=";
	
	for(var i=0;i<items.length;i++)
	{
		var item = items[i];
		while(item.charAt(0)==' ')
			item = item.substring(1, item.length);
		if(item.indexOf(name_is) == 0)
			return item.substring(name_is.length, item.length);
	}
	return null;
}

var rid;
function refreshChange()
{
	opt = document.getElementById('refresh');
	
	if(rid)
		clearInterval(rid);
	if(opt.value > 0)
		rid = setInterval("doRefresh()", opt.value*1000);
	document.cookie = 'refresh_int='+opt.value;
}
function doRefresh()
{
	inputs = document.getElementsByTagName('input');
	
	for(var i=0;i<inputs.length;i++)
	{
		if(inputs[i].type != 'checkbox')
			continue;
		if(inputs[i].checked)
			return;
	}
	
	mform = document.getElementById('mainform');
	mform.submit();
}

function loadRefresh()
{
	rf = readCookie('refresh_int');
	if(rf)
	{
		opt = document.getElementById('refresh');
		opt.value = rf;
		refreshChange();
	}
}
