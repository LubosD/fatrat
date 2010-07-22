var client;
var rpcMethods = ["getQueues", "Queue.getTransfers", "Transfer.setProperties", "Transfer.delete", "Queue.moveTransfers"];
var queues, transfers;
var currentQueue, currentTransfers = [];
var interval;

function clientInit() {
	client = XmlRpc.getObject("/xmlrpc", rpcMethods);
	
	$("#toolbar-add").click(actionAdd);
	$("#toolbar-delete").click(actionDelete);
	$("#toolbar-delete-with-data").click(actionDeleteWithData);
	$("#toolbar-delete-completed").click(actionDeleteCompleted);
	$("#toolbar-resume").click(function() { sendStateChange('Waiting'); });
	$("#toolbar-force-resume").click(function() { sendStateChange('ForcedActive'); });
	$("#toolbar-pause").click(function() { sendStateChange('Paused'); });
	$("#toolbar-move-top").click(actionMoveTop);
	$("#toolbar-move-up").click(actionMoveUp);
	$("#toolbar-move-down").click(actionMoveDown);
	$("#toolbar-move-bottom").click(actionMoveBottom);
	$("#toolbar-queue-add").click(actionAddQueue);
	$("#toolbar-queue-delete").click(actionDeleteQueue);
	
	updateQueues();
	
	iv = $.cookie("refreshInterval");
	if (!iv)
		iv = 2;
	$("#refresh").val(''+iv);
	interval = window.setInterval(updateQueues, iv*1000);
	
	$("#refresh").change(function() {
		val = $(this).val();
		$.cookie("refreshInterval", val, { expires: 90 } );
		window.clearInterval(interval);
		interval = window.setInterval(updateQueues, val*1000);
	});
}

function updateQueues() {
	client.getQueues(function(data) {
		queues = data;
		
		i = 0;
		$('#queues .queue-item').each(function() {
			if (i >= queues.length)
				$(this).remove();
			else {
				if (this.id != queues[i].uuid)
					this.id = queues[i].uuid;
				if (queues[i].uuid != currentQueue)
					$(this).removeClass('ui-selected');
				$(this).text(queues[i].name);
			}
			i++;
		});
		
		for(;i<queues.length;i++) {
			addQueueItem(queues[i]);
		}
		
		if (currentQueue) {
			q = document.getElementById(currentQueue);
			if (q) {
				$(q).addClass("ui-selected");
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
		
		i = 0;
		$('#transfers .transfer-item').each(function() {
			if (i >= transfers.length)
				$(this).remove();
			else {
				if (currentTransfers.indexOf(transfers[i].uuid) == -1)
					$(this).removeClass('ui-selected');
				fillTransferRow(this, transfers[i]);
			}
			i++;
		});
		
		for(;i<transfers.length;i++) {
			addTransferItem(transfers[i]);
		}
		
		for (x=0;x<currentTransfers.length;x++) {
			t = document.getElementById(currentTransfers[x]);
			if (t)
				$(t).addClass("ui-selected");
			else {
				currentTransfers.splice(x, 1);
				x--;
			}
		}
		
		transfersSelectionChanged();
		tabSwitched(false);
	});
}

function tabSwitched(reallySwitched) {
	d = new Date();
	if ($("#tabs-tsg").is(':visible')) {
		
		if (currentTransfers.length == 1) {
			if (reallySwitched)
				$("#tabs-tsg-img").attr('style','visibility: hidden');
			$("#tabs-tsg-img").attr('src', '/generate/graph.png?'+currentTransfers[0]+'&'+d.getTime());
		} else
			$("#tabs").tabs("option", "selected", 0);
	}
	if ($("#tabs-qsg").is(':visible')) {
		if (currentQueue) {
			if (reallySwitched)
				$("#tabs-qsg-img").attr('style','visibility: hidden');
			$("#tabs-qsg-img").attr('src', '/generate/qgraph.png?'+currentQueue+'&'+d.getTime());
		} else
			$("#tabs").tabs("option", "selected", 0);
	}
	if ($("#global-log").is(':visible')) {
		$.get('/log', function(data) {
			$("#global-log").text(data);
			if (reallySwitched)
				$("#global-log").scrollTop($("#global-log")[0].scrollHeight);
		});
		if (currentTransfers.length == 1) {
			$.get('/log/'+currentTransfers[0], function(data) {
				$("#transfer-log").text(data);
				if (reallySwitched)
					$("#transfer-log").scrollTop($("#transfer-log")[0].scrollHeight);
			});
		} else {
			$("#transfer-log").html('');
		}
	}
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
	if (row.id != data.uuid)
		row.id = data.uuid;
	tds = row.getElementsByTagName('td');
	progress = 0;
	
	if (data.total > 0)
		progress = singleDecimalDigit(100.0*data.done/data.total);
	
	span = tds[0].getElementsByTagName('span');
	if (span[0].innerHTML != data.name)
		span[0].innerHTML = data.name;
	
	span = tds[1].getElementsByTagName('span');
	if (span && span.length > 0) {
		text = progress+' %';
		if (text != span[0].innerHTML) {
			span[0].innerHTML = text;
			$(tds[1]).progressbar({ value: progress });
		}
	} else {
		tds[1].innerHTML = '<span>'+progress + ' %</span>';
		$(tds[1]).progressbar({ value: progress });
	}
	
	if (data.total > 0)
		txt = formatSize(data.total);
	else
		txt = '';
	if (txt != tds[2].innerHTML)
		tds[2].innerHTML = txt;
	
	isactive = data.state == 'Active' || data.state == 'ForcedActive';
	if (data.speeds[0] > 0 || isactive)
		txt = formatSize(data.speeds[0])+'/s';
	else
		txt = '';
	if (txt != tds[3].innerHTML)
		tds[3].innerHTML = txt;
	
	if (data.speeds[1] > 0 || (isactive && data.mode == 'Upload'))
		txt = formatSize(data.speeds[1])+'/s';
	else
		txt = '';
	if (txt != tds[4].innerHTML)
		tds[4].innerHTML = txt;
	
	var speed;
	if (data.primaryMode == 'Download')
		speed = data.speeds[0];
	else
		speed = data.speeds[1];
	if (isactive && data.mode == data.primaryMode && speed > 0)
		txt = formatTime( (data.total - data.done) / speed );
	else
		txt = '';
	if (txt != tds[5].innerHTML)
		tds[5].innerHTML = txt;
	
	if (data.message != tds[6].innerHTML)
		tds[6].innerHTML = data.message;
	
	cls = 'fatrat-transfer-state fatrat-transfer-'+data.state.toLowerCase();
	if (data.mode == 'Upload' && (data.primaryMode == 'Upload' || data.state != 'Completed'))
		cls = cls + '-ul';
	if (tds[0].getAttribute('class') != cls)
		tds[0].setAttribute('class', cls);
}

function addTransferItem(item, addBefore) {
	ndiv = document.createElement('tr');
	ndiv.setAttribute('class', 'transfer-item ui-selectee');
	ndiv.setAttribute('id', item.uuid);
	
	for(var j=0;j<7;j++) {
		td = document.createElement('td');
		if (j == 0) {
			img = document.createElement('img');
			img.setAttribute('alt', 'Properties');
			img.setAttribute('title', 'Properties');
			img.setAttribute('src', 'css/icons/queue/properties.png');
			img.setAttribute('class', 'transfer-properties');
			$(img).click(function() {
				transferProperties(this.parentNode.parentNode.id);
			});
			
			td.appendChild(img);
			td.appendChild(document.createElement('span'));
		} else if (j == 1)
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
	
	var dtabs = [];
	if (ntransfers.length != 1)
		dtabs = [1, 2];
	$('#tabs').tabs("option", "disabled", dtabs);
}

function sendStateChange(newState) {
	if (!currentTransfers.length)
		return;
	
	properties = { state: newState };
	client.Transfer_setProperties(currentTransfers, properties, function(data) {
		updateTransfers();
	});
}

function actionAdd() {
}

function actionDelete() {
	if (currentTransfers.length == 0)
		return;
	
	$("#delete-dialog").dialog({
		resizable: false,
		height:190,
		modal: true,
		buttons: {
			'No': function() {
				$(this).dialog('close');
			},
			'Yes': function() {
				$(this).dialog('close');
				
				client.Transfer_delete(currentTransfers, false, function(data) {
					updateTransfers();
					$("#tabs").tabs("option", "selected", 0);
				});
			}
		}
	});
}

function actionDeleteWithData() {
	if (currentTransfers.length == 0)
		return;
	
	$("#delete-dialog-with-data").dialog({
		resizable: false,
		height:190,
		modal: true,
		buttons: {
			'No': function() {
				$(this).dialog('close');
			},
			'Yes': function() {
				$(this).dialog('close');
				
				client.Transfer_delete(currentTransfers, true, function(data) {
					updateTransfers();
					$("#tabs").tabs("option", "selected", 0);
				});
			}
		}
	});
}

function actionDeleteCompleted() {
	toRemove = [];
	for (b=0;b<transfers.length;b++) {
		if (transfers[b].state == 'Completed')
			toRemove.push(transfers[b].uuid);
	}
	
	client.Transfer_delete(toRemove, false, function(data) {
		updateTransfers();
		$("#tabs").tabs("option", "selected", 0);
	});
}

function actionMove(dir) {
	if (currentTransfers.length == 0)
		return;
	
	client.Queue_moveTransfers(currentQueue, currentTransfers, dir, function(data) {
		updateTransfers();
	});
}

function actionMoveTop() {
	actionMove('top');
}

function actionMoveUp() {
	actionMove('up');
}

function actionMoveDown() {
	actionMove('down');
}

function actionMoveBottom() {
	actionMove('bottom');
}

function actionAddQueue() {
}

function actionDeleteQueue() {
}

function transferProperties(uuid) {
	var xtr;
	
	for (rz=0;rz<transfers.length;rz++) {
		if (transfers[rz].uuid == uuid)
			xtr = transfers[rz];
	}
	if (!xtr)
		return;
	
	$("#transfer-properties-target").val(xtr.object);
	$("#transfer-properties-comment").val(xtr.comment);
	$("#transfer-properties-speed-down").val(xtr.userSpeedLimits[0]/1024);
	$("#transfer-properties-speed-up").val(xtr.userSpeedLimits[1]/1024);
	
	$("#transfer-properties").dialog({
		resizable: true,
		height:400,
		width:400,
		modal: true,
		buttons: {
			Cancel: function() {
				$(this).dialog('close');
			},
			Ok: function() {
				obj = $("#transfer-properties-target").val();
				down = parseInt($("#transfer-properties-speed-down").val());
				up = parseInt($("#transfer-properties-speed-up").val());
				cmt = $("#transfer-properties-comment").val();
				
				if (obj == '' || isNaN(down) || isNaN(up) || down < 0 || up < 0) {
					alert('Please enter valid values!');
					return;
				}
				
				$(this).dialog('close');
				
				properties = { object: obj, userSpeedLimits: [down*1024,up*1024], comment: cmt };
				client.Transfer_setProperties([uuid], properties, function(data) {
					updateTransfers();
				});
			}
		}
	});
}
	
