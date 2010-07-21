var client;
var rpcMethods = ["getQueues", "Queue.getTransfers", "Transfer.setProperties"];
var queues, transfers;
var currentQueue, currentTransfers = [];

function clientInit() {
	client = XmlRpc.getObject("/xmlrpc", rpcMethods);
	updateQueues();
}

function updateQueues() {
	client.getQueues(function(data) {
		queues = data;
		added = [];
		
		$('#queues .queue-item').each(function() {
			xid = $(this).attr('id');
			found = false;
			for (q in queues) {
				if (q.uuid == xid) {
					found = true;
					
					// update the name
					$(this).html(q.name);
					added.push(q.uuid);
					
					break;
				}
			}
			if (!found)
				$(this).remove();
		});
		
		i = 0;
		$('#queues .queue-item').each(function() {
			xid = $(this).attr('id');
			while(queues[i].uuid != xid) {
				q = queues[i];
				if (added.indexOf(q.uuid) != -1)
					$('#'+q.uuid).remove();
				else
					added.push(q.uuid);
				addQueueItem(q, this); // before this
				i++;
			}
		});
		
		for(;i<queues.length;i++) {
			addQueueItem(queues[i]);
		}
		
		if (currentQueue) {
			q = $('#'+currentQueue);
			if (q.length == 1) {
				q.addClass("ui-selected");
				updateTransfers();
				return;
			} else
				currentQueue = undefined;
		}
		x = $(".queue-item").first().click();
	});
}

function addQueueItem(item, addBefore) {
	ndiv = document.createElement('div');
	ndiv.setAttribute('class', 'queue-item ui-widget-content ui-selectee ui-hoverable');
	ndiv.setAttribute('id', item.uuid);
	ndiv.innerHTML = item.name;
	
	if (addBefore)
		document.getElementById('queues').insertBefore(ndiv, addBefore);
	else
		document.getElementById('queues').appendChild(ndiv);
	$(ndiv).click(function(e){
		$(this).toggleClass("ui-selected").siblings().removeClass("ui-selected");
		queueClicked(e.target);
	});
}

function queueClicked(elem) {
	old = currentQueue;
	currentQueue = elem.id;
	if (currentQueue != old)
		updateTransfers();
}

function updateTransfers() {
	if (!currentQueue) {
		$("#transfers .transfer-item").remove();
		return;
	}
	client.Queue_getTransfers(currentQueue, function(data) {
		transfers = data;
		added = [];
		
		$('#transfers .transfer-item').each(function() {
			xid = $(this).attr('id');
			found = false;
			for (t in transfers) {
				if (t.uuid == xid) {
					found = true;
					
					// update all properties
					fillTransferRow(this, t);
					added.push(t.uuid);
					
					break;
				}
			}
			
			if (!found)
				$(this).remove();
		});
		
		i = 0;
		$('#transfers .transfer-item').each(function() {
			xid = $(this).attr('id');
			while(transfers[i].uuid != xid) {
				t = transfers[i];
				if (added.indexOf(t.uuid) != -1)
					$('#'+t.uuid).remove();
				else
					added.push(t.uuid);
				addTransferItem(t, this); // before this
				i++;
			}
		});
		
		for(;i<transfers.length;i++) {
			addTransferItem(transfers[i]);
		}
		
		for (x=0;x<currentTransfers.length;x++) {
			t = $('#'+currentTransfers[x]);
			if (t.length == 1)
				t.addClass("ui-selected");
			else {
				currentTransfers.splice(x, 1);
				x--;
			}
		}
		
		transfersSelectionChanged();
	});
}

function singleDecimalDigit(num) {
	return Math.round(num*10)/10;
}

function formatSize(bytes) {
	if (bytes < 1024)
		return bytes + ' B';
	else if (bytes < 1024*1024)
		return singleDecimalDigit(bytes/1024.0) + ' KB';
	else if (bytes < 1024*1024*1024)
		return singleDecimalDigit(bytes/1024.0/1024.0) + ' MB';
	else
		return singleDecimalDigit(bytes/1024.0/1024.0/1024.0) + ' GB';
}

function formatTime(seconds) {
	s = seconds % 60;
	seconds -= s;
	m = seconds % (60*60);
	seconds -= m*60;
	h = seconds % (60*60*60);
	seconds -= h*60*60;
	d = Math.round(seconds/60/60/60/24);
	
	rv = '';
	if (d > 0)
		rv += d+'d ';
	if (h > 0)
		rv += h+'h ';
	if (m > 0)
		rv += m+'m ';
	if (s > 0)
		rv += s+'s';
	return rv;
}

function fillTransferRow(row, data) {
	//alert(data.toSource());
	tds = row.getElementsByTagName('td');
	progress = 0;
	
	if (data.total > 0)
		progress = singleDecimalDigit(100.0*data.done/data.total);
	
	tds[0].innerHTML = data.name;
	tds[1].innerHTML = '<span>'+progress + ' %</span>';
	$(tds[1]).progressbar({ value: progress });
	
	if (data.total > 0)
		tds[2].innerHTML = formatSize(data.total);
	else
		tds[2].innerHTML = '';
	
	isactive = data.state == 'Active' || data.state == 'ForcedActive';
	if (data.speeds[0] > 0 || isactive)
		tds[3].innerHTML = formatSize(data.speeds[0])+'/s';
	else
		tds[3].innerHTML = '';
	if (data.speeds[1] > 0 || (isactive && data.mode == 'Upload'))
		tds[4].innerHTML = formatSize(data.speeds[1])+'/s';
	else
		tds[4].innerHTML = '';
	
	var speed;
	if (data.primaryMode == 'Download')
		speed = data.speeds[0];
	else
		speed = data.speeds[1];
	if (isactive && data.mode == data.primaryMode && speed > 0)
		tds[5].innerHTML = formatTime( (data.total - data.done) / speed );
	else
		tds[5].innerHTML = '';
	tds[6].innerHTML = data.message;
	
	cls = 'fatrat-transfer-state fatrat-transfer-'+data.state.toLowerCase();
	if (data.mode == 'Upload' && (data.primaryMode == 'Upload' || data.state != 'Completed'))
		cls = cls + '-ul';
	tds[0].setAttribute('class', cls);
}

function addTransferItem(item, addBefore) {
	ndiv = document.createElement('tr');
	ndiv.setAttribute('class', 'transfer-item ui-selectee');
	ndiv.setAttribute('id', item.uuid);
	
	for(var j=0;j<7;j++) {
		td = document.createElement('td');
		if (j == 1)
			td.setAttribute('class', 'progressbar');
		ndiv.appendChild(td);
	}
	
	fillTransferRow(ndiv, item);
	
	if (addBefore)
		$(addBefore).before(ndiv);
	else
		$('#transfers tbody').append(ndiv);
}

function enableButton(btn, enable) {
	if (enable) {
		btn.removeAttr('disabled');
		btn.removeClass('ui-state-disabled');
	} else {
		btn.attr('disabled', 'disabled');
		btn.addClass('ui-state-disabled');
	}
}

function transferResumable(state) {
	return state != 'Waiting' && state != 'Active';
}
function transferPausable(state) {
	return state != 'Paused';
};
function getTransfer(uuid) {
	for (r=0;r<transfers.length;r++) {
		if (transfers[r].uuid == uuid)
			return transfers[r];
	}
	return undefined;
}
function getTransferPosition(uuid) {
	for (r=0;r<transfers.length;r++) {
		if (transfers[r].uuid == uuid)
			return r;
	}
	return -1;
}

function transfersSelectionChanged() {
	ntransfers = [];
	$("#transfers .ui-selected").each(function() {
		ntransfers.push($(this).attr('id'));
	});
	currentTransfers = ntransfers;

	enableButton($('#toolbar-delete'), ntransfers.length > 0);
	enableButton($('#toolbar-delete-with-data'), ntransfers.length > 0);
	enableButton($('#toolbar-force-resume'), ntransfers.length > 0);
	
	resumable = false;
	pausable = false;
	up = false;
	down = false;
	
	for (u=0;u<currentTransfers.length;u++) {
		tpos = getTransferPosition(currentTransfers[u]);
		tt = transfers[tpos];
		resumable |= transferResumable(tt.state);
		pausable |= transferPausable(tt.state);
		up |= tpos > 0;
		down |= tpos < transfers.length-1;
	}
	enableButton($('#toolbar-resume'), resumable);
	enableButton($('#toolbar-pause'), pausable);
	enableButton($('#toolbar-move-up'), up);
	enableButton($('#toolbar-move-top'), up);
	enableButton($('#toolbar-move-down'), down);
	enableButton($('#toolbar-move-bottom'), down);
}

