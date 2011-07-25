var client;
var rpcMethods = ["getQueues", "getTransferClasses", "Queue.getTransfers",
	"Transfer.setProperties", "Transfer.getAdvancedProperties", "Transfer.delete",
	"Queue.moveTransfers", "Queue.setProperties", "Queue.create", "getTransferClasses",
	"Queue.addTransfers", "Queue.addTransferWithData", "Settings.apply",
	"Settings.setValue", "Settings.getValue", "Settings.getPages", "Transfer.getSpeedData",
	"Queue.getSpeedData"  ];
var queues, transfers;
var currentQueue, currentTransfers = [];
var interval, graphMinutes = 5;
var transferClasses, settingsPages;
var settingsStore = [];

function clientInit() {
	client = XmlRpc.getObject("/xmlrpc", rpcMethods);
	
	client.getTransferClasses(function(data) {
		transferClasses = data;
		var options = $("#new-transfer-class").attr('options');
		
		$.each(transferClasses, function(index, cls) {
			if (cls.mode == "Download")
				options[options.length] = new Option(cls.longName, cls.shortName, true, true);
		});
	});
	client.Settings_getPages(function(data) {
		settingsPages = data;
		
		var list = $("#settings-list");
		for(var i=0;i<settingsPages.length;i++) {
			list.append('<div rel="'+i+'" class="settings-item ui-widget-content"><img src="'+settingsPages[i].icon+'" alt="" /><span>'+settingsPages[i].title+'</span></div>');
		}
		
		$("#settings-list div").click(function() {
			$(this).addClass("ui-selected").siblings().removeClass("ui-selected");
			switchSettingsPage(false);
		});
	});
	getSettingsValues(["graphminutes"], function(d) {
		graphMinutes = d["graphminutes"];
	});
	
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
	$("#toolbar-settings").click(actionSettings);
	
	updateQueues();
	window.setTimeout('startCaptchaListener()', 1000); // hack for Chrome
	
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
				this.innerHTML = queues[i].name;
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
	$(ndiv).dblclick(function(e){
		cq = getQueue(currentQueue);
		$("#queue-properties-name").val(cq.name);
		$("#queue-properties-target-directory").val(cq.defaultDirectory);
		$("#queue-properties-move-directory").val(cq.moveDirectory);
		$("#queue-properties-move").attr("checked", (cq.moveDirectory == "")?"":"checked");
		$("#queue-properties-move-directory").attr("disabled", (cq.moveDirectory != "")?"":"disabled");
		$("#queue-properties-speed-down").val(cq.speedLimits[0]/1024);
		$("#queue-properties-speed-up").val(cq.speedLimits[1]/1024);
		
		tcl = cq.transferLimits[0] > 0 || cq.transferLimits[1] > 0;
		$("#queue-properties-count-down, #queue-properties-count-up, #queue-propeties-upasdown").attr("disabled", (tcl)?"":"disabled");
		$("#queue-propeties-upasdown").attr("checked", cq.upAsDown ? "checked" : "");
		$("#queue-properties-transfer-limits").attr("checked", tcl ? "checked" : "");
		
		if (tcl) {
			$("#queue-properties-count-down").val(cq.transferLimits[0]);
			if (!cq.upAsDown) {
				$("#queue-properties-count-up").val(cq.transferLimits[1]);
				$("#queue-properties-count-up").attr("disabled", "");
			} else {
				$("#queue-properties-count-up").val("1");
				$("#queue-properties-count-up").attr("disabled", "disabled");
			}
		} else {
			$("#queue-properties-count-down").val("1");
			$("#queue-properties-count-up").val("1");
		}
		
		$("#queue-properties").dialog({
			resizable: true,
			//height: 500,
			width: 600,
			modal: true,
			buttons: {
				Cancel: function() {
					$(this).dialog('close');
				},
				OK: function() {
					properties = getQueueDlgPropertyMap();
					
					$(this).dialog('close');
					client.Queue_setProperties(currentQueue, properties, function(data) {
						updateQueues();
					});
				}
			}
		});
	});
	$(ndiv).click(function(e){
		$(this).toggleClass("ui-selected").siblings().removeClass("ui-selected");
		queueClicked(e.target);
	});
}

function getQueueDlgPropertyMap() {
	sdown = parseInt($("#queue-properties-speed-down").val());
	sup = parseInt($("#queue-properties-speed-up").val());
	
	properties = {
		name: $("#queue-properties-name").val(),
		upAsDown : $("#queue-propeties-upasdown").is(":checked")
	};
	dir = $("#queue-properties-target-directory").val();
	if (dir != "")
		properties.defaultDirectory = dir;
	
	if (properties.name == "") {
		alert("Enter queue name");
		return null;
	}
	
	properties.moveDirectory = $("#queue-properties-move").is(":checked") ? $("#queue-properties-move-directory").val() : "";
	
	if (!isNaN(sup) && !isNaN(sdown) && sup >= 0 && sdown >= 0)
		properties.speedLimits = [ sdown*1024, sup*1024 ];
	
	tdown = tup = -1;
	if ($("#queue-properties-transfer-limits").is(":checked")) {
		tdown = parseInt($("#queue-properties-count-down").val());
		if (properties.upAsDown)
			tup = tdown;
		else
			tup = parseInt($("#queue-properties-count-up").val());
		if (isNaN(tdown) || tdown <= 0)
			tdown = 1;
		if (isNaN(up) || tup <= 0)
			tup = 1;
	}
	properties.transferLimits = [ tdown, tup ];
	
	return properties;
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
		clearError();
	});
}

function tabSwitched(reallySwitched) {
	d = new Date();
	if ($("#tabs-tsg").is(':visible')) {
		
		if (currentTransfers.length == 1) {
			if (reallySwitched)
				$("#tabs-tsg-img").attr('style','visibility: hidden');
			client.Transfer_getSpeedData(currentTransfers[0], function(data) {
				
				drawSpeedGraph("tabs-tsg-img", data);
				$("#tabs-tsg-img").attr('style','visibility: visible');
				
			});
		} else
			$("#tabs").tabs("option", "selected", 0);
	}
	else if ($("#tabs-qsg").is(':visible')) {
		if (currentQueue) {
			if (reallySwitched)
				$("#tabs-qsg-img").attr('style','visibility: hidden');
			client.Queue_getSpeedData(currentQueue, function(data) {
				drawSpeedGraph("tabs-qsg-img", data);
				$("#tabs-qsg-img").attr('style','visibility: visible');
			});
		} else
			$("#tabs").tabs("option", "selected", 0);
	}
	else if ($("#global-log").is(':visible')) {
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
	else if ($("#tabs-details").is(':visible')) {
		var text = "";
		t = getTransfer(currentTransfers[0]);
		
		if (reallySwitched)
		{
			$('#details-subclass')[0].innerHTML = '';
			
			if (t.detailsScript)
			{
				sc = document.createElement('script');
				sc.type = 'text/javascript';
				sc.src = t.detailsScript;
				$('#details-subclass').append(sc);
			}
		} else if (t.detailsScript && typeof subclassPerformReload == "function") {
			subclassPerformReload(t);
		}
		
		if (t.primaryMode == 'Upload')
			text = '<div class="ui-state-error">This feature is available only for download-oriented transfers.</div>';
		else {
			if (t.state != 'Completed' && t.mode == 'Download')
				text = '<div class="ui-state-error">The download may be incomplete.</div>';
			
			if (t.dataPathIsDir) {
				//text = 'DataPathIsDir - not implemented yet';
				if (reallySwitched) {
					$("#details-download-tree").html('');
					$("#details-download-tree").jstree({
						plugins: ["themes", "xml_data", "types", "ui"],
						xml_data: {
							ajax: {
								url: "/browse/"+currentTransfers[0],
								data: function(n) {
									return {
										path: n.attr ? n.attr("id") : ""
									}
								}
							}
						},
						"types" : {
							"type_attr": "type",
							"max_depth" : -2,
							"max_children" : -2,
							"valid_children" : [ "folder", "file" ],
							"types" : {
								"folder" : {
									"valid_children" : [ "default", "folder" ],
									"icon" : {
										"image" : "css/jstree/folder.png"
									},
									select_node: function() { return false; },
									hover_node: false
								},
								"default" : {
									"valid_children" : "none",
									"icon" : {
										"image" : "css/jstree/file.png"
									},
									"select_node": function() {
										window.location = '/download?transfer='+currentTransfers[0]+'&path='+$(".jstree-hovered")[0].parentNode.id;
										return true; 
									},
									"hover_node": true
								}
							}
						}
					});
				}
			} else {
				text += '<a class="downlink" href="/download?transfer='+currentTransfers[0]+'">Download the file ('+t.name+')</a>';
				$("#details-download-tree").html('');
			}
		}
		
		//if (text != "") {
			//$("#details-download-tree").html('');
			$("#details-download").html(text);
			$("#details-download .downlink").button();
		//} else
		//	$("#details-download").html('');
		if (reallySwitched)
			$('#accordion').accordion({ active: ((t.detailsScript)?1:0) });
	}
}

function singleDecimalDigit(num) {
	return Math.floor(num*10)/10;
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
	d = Math.floor(seconds/60/60/24);
	seconds -= d*60*60*24;
	h = Math.floor(seconds/60/60);
	seconds -= h*60*60;
	m = Math.floor(seconds/60);
	seconds -= m*60;
	s = Math.floor(seconds);
	
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
	if (isactive && data.mode.toString() == data.primaryMode.toString() && speed > 0)
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
		if (j == 0)
			td.appendChild(document.createElement('span'));
		else if (j == 1)
			td.setAttribute('class', 'progressbar');
		ndiv.appendChild(td);
	}
	
	fillTransferRow(ndiv, item);
	$(ndiv).mousedown(function() {
		// To ensure right clicks work the way users expect them to
		$(ndiv).addClass('ui-selected').siblings().removeClass('ui-selected');
		transfersSelectionChanged();
	});
	
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

function enableMenuItem(item, enable) {
	if (enable)
		$("#menu-transfer").enableContextMenuItems(item);
	else
		$("#menu-transfer").disableContextMenuItems(item);
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
function getQueue(uuid) {
	for (wr=0;wr<queues.length;wr++) {
		if (queues[wr].uuid == uuid)
			return queues[wr];
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
	enableMenuItem('#menu-delete', ntransfers.length > 0);
	enableButton($('#toolbar-delete-with-data'), ntransfers.length > 0);
	enableMenuItem('#menu-delete-data', ntransfers.length > 0);
	enableButton($('#toolbar-force-resume'), ntransfers.length > 0);
	enableMenuItem('#menu-force-resume', ntransfers.length > 0);
	enableMenuItem('#menu-properties', ntransfers.length == 1);
	
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
	enableMenuItem('#menu-resume', resumable);
	enableButton($('#toolbar-pause'), pausable);
	enableMenuItem('#menu-pause', pausable);
	enableButton($('#toolbar-move-up'), up);
	enableMenuItem('#menu-move-up', up);
	enableButton($('#toolbar-move-top'), up);
	enableMenuItem('#menu-move-to-top', up);
	enableButton($('#toolbar-move-down'), down);
	enableMenuItem('#menu-move-down', down);
	enableButton($('#toolbar-move-bottom'), down);
	enableMenuItem('#menu-move-to-bottom', down);
	
	var dtabs = [];
	if (ntransfers.length != 1)
		dtabs = [1, 2];
	$('#tabs').tabs("option", "disabled", dtabs);
}

function sendStateChange(newState) {
	if (!currentTransfers.length)
		return;
	
	if (newState != 'Waiting') {
		var properties = { state: newState };
		client.Transfer_setProperties(currentTransfers, properties, function(data) {
			updateTransfers();
		});
	} else {
		var active = [];
		var inactive = [];
		
		for (var i = 0; i < currentTransfers.length; i++) {
			if (currentTransfers[i].state == 'ForcedActive')
				active.push(currentTransfers[i]);
			else if (currentTransfers[i].state != 'Active')
				inactive.push(currentTransfers[i]);
		}
		
		if (active.length > 0) {
			client.Transfer_setProperties(active, { state: 'Active' }, function(data) {
				if (inactive.length > 0)
					updateTransfers();
			});
		}
		if (inactive.length > 0) {
			client.Transfer_setProperties(inactive, { state: 'Waiting' }, function(data) {
				updateTransfers();
			});
		}
	}
}

function actionAdd(link) {
	var options = $("#new-transfer-destination-queue").attr('options');
	
	options.length = 0;
	$.each(queues, function(index, queue) {
		options[options.length] = new Option(queue.name, queue.uuid, true, true);
	});
	
	if (currentQueue)
		$("#new-transfer-destination-queue").val(currentQueue);
	
	if (!window.FileReader)
		$('#new-transfer-file').attr('disabled', 'disabled');
	$('#new-transfer-class').val('');
	$('#new-transfer-paused').attr('checked', '');
	$('#new-transfer-speed-down').val('0');
	$('#new-transfer-speed-up').val('0');
	$('#new-transfer-target').val(getQueue(currentQueue).defaultDirectory);
	
	if (typeof link == "string")
		$('#new-transfer-links').val(link);
	else
		$('#new-transfer-links').val('');
	$('#new-transfer-file').val('');

	$("#new-transfer").dialog({
		resizable: true,
		//height: 500,
		width: 600,
		modal: true,
		open: function() { $('#new-transfer-links').focus(); },
		buttons: {
			Cancel: function() {
				$(this).dialog('close');
			},
			OK: function() {
				_class = $('#new-transfer-class').val();
				target = $('#new-transfer-target').val();
				paused = $('#new-transfer-paused').is(':checked');
				down = $('#new-transfer-speed-down').val() * 1024;
				up = $('#new-transfer-speed-up').val() * 1024;
				queue = $('#new-transfer-destination-queue').val();
				
				if (target == "" || !queue)
					return;
				
				if (down < 0) down = 0;
				if (up < 0) up = 0;
				
				$(this).dialog('close');
				
				// send URLs
				urls = $('#new-transfer-links').val().split(/\s+/g);
				if (urls.length > 0 && urls[0] != "") {
					client.Queue_addTransfers(false, queue, urls, _class, target, paused, up, down);
				}
				
				// send files
				if (window.FileReader) {
					var file = document.getElementById('new-transfer-file').files[0];
					if (file) {
						if (file.size > 3*1024*1024) {
							alert("The file is unusually large, aborting the operation");
							return;
						}
						reader = new FileReader();
						reader.onload = function(e) {
							bd = new BinaryData(e.target.result);
							client.Queue_addTransferWithData(false, queue, removePath(file.name), bd, _class, target, paused, up, down);
							updateTransfers();
						};
						reader.readAsBinaryString(file);
					} else
						updateTransfers();
				} else
					updateTransfers();
			}
		}
	});
}

function removePath(path) {
	p = path.lastIndexOf(path);
	if (p == -1)
		return path;
	else
		return path.substr(p);
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
	$("#queue-properties-name").val("");
	$("#queue-properties-target-directory").val("");
	$("#queue-properties-move-directory").val("");
	$("#queue-properties-move").attr("checked", "");
	$("#queue-properties-move-directory").attr("disabled", "disabled");
	$("#queue-properties-speed-down").val(0);
	$("#queue-properties-speed-up").val(0);
	
	$("#queue-properties-count-down, #queue-properties-count-up, #queue-propeties-upasdown").attr("disabled", "disabled");
	$("#queue-propeties-upasdown").attr("checked", "");
	$("#queue-properties-transfer-limits").attr("checked", "");
	
	$("#queue-properties-count-down").val("1");
	$("#queue-properties-count-up").val("1");
	
	$("#queue-properties").dialog({
		resizable: true,
		//height: 500,
		width: 600,
		modal: true,
		buttons: {
			Cancel: function() {
				$(this).dialog('close');
			},
			OK: function() {
				properties = getQueueDlgPropertyMap();
				
				if (properties)
				{
					$(this).dialog('close');
					client.Queue_create(properties, function(data) {
						updateQueues();
					});
				}
			}
		}
	});
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
			OK: function() {
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

function showError(e) {
	var msg;
	if (e.message)
		msg = e.message;
	else if (e.faultString)
		msg = e.faultString;
	else
		msg = e.toString();
	$("#errors").text(msg);
	$("#errors").show('slow');
	$.unblockUI();
}

	
function clearError() {
	$("#errors").hide('slow');
}

function actionSettings() {
	settingsStore = [];
	$('#settings-pane-script').html('');
	
	$("#settings").dialog({
		resizable: false,
		height:500,
		width:700,
		modal: true,
		open: function() { $("#settings-list .settings-item").first().addClass("ui-selected").siblings().removeClass("ui-selected"); switchSettingsPage(true); },
		buttons: {
			Cancel: function() {
				$("#settings-pane").html('');
				$(this).dialog('close');
			},
			OK: function() {
				var keys = [];
				var values = [];
				
				if (typeof settingsSave == 'function') {
					try {
						settingsSave();
					} catch (ex) {
						alert(ex);
						return;
					}
				}
				
				for (key in settingsStore) {
					keys.push(key);
					values.push(settingsStore[key]);
				}
				
				tthis = $(this);
				client.Settings_setValue(keys, values, function() {
					client.Settings_apply();
					$("#settings-pane").html('');
					tthis.dialog('close');
				});
			}
		}
	});
}

function getSettingsValues(keys, fn) {
	toFetch = [];
	for (var i=0;i<keys.length;i++) {
		if (!(keys[i] in settingsStore))
			toFetch[toFetch.length] = keys[i];
	}
	client.Settings_getValue(toFetch, function(data) {
		rdata = settingsStore;
		for (var i=0;i<data.length;i++)
			rdata[toFetch[i]] = data[i];
		fn(rdata);
	});
}

function setSettingsValue(key, value) {
	settingsStore[key] = value;
}

function switchSettingsPage(nosave) {
	if (!nosave && typeof settingsSave == 'function') {
		try {
			settingsSave();
		} catch (ex) {
			alert(ex);
			return;
		}
	}
	
	id = $("#settings-list .ui-selected").attr("rel");
	
	sc = document.createElement('script');
	sc.type = 'text/javascript';
	sc.src = settingsPages[id].script;
	$('#settings-pane-script').html('');
	$('#settings-pane-script').append(sc);

}

function isTrue(val) {
	return val == "true" || val == true;
}

function checkInt(varid) {
	el = $('#'+varid);
	v = parseInt(el.val().replace(" ", ""));
	
	if (isNaN(v))
		throw "String entered instead of an integer";
	
	min = el.attr('min');
	if (min != undefined && min != "" && min > v)
		throw "Integer value out of permitted range: "+varid+", min="+min;
	max = el.attr('max');
	if (max != undefined && max != "" && max < v)
		throw "Integer value out of permitted range: "+varid+", max="+max;
	
	return v;
}

function handleCaptchaEvent(data) {
	var pos = data.indexOf(",");
	var id = data.substring(0, pos);
	var url = data.substring(pos+1);
	
	/*if (window.webkitNotifications && window.webkitNotifications.checkPermission() == 0) {
		var not = window.webkitNotifications.createNotification('', 'Captcha', 'There is a captcha to be solved in FatRat');
		//var not = window.webkitNotifications.createHTMLNotification('captcha.html?url='+url+'&id='+id);
		not.show();
	}*/
	
	var win;
	try {
		var date = new Date();
		win = window.open('captcha.html?url='+url+'&id='+id+'&timeStart='+date.getTime(), '_blank', 'width=300,height=250,top=300,left=400');
		if (win == null || typeof(win) == 'undefined') {
			throw "blocked";
		}
	} catch (e) {
		alert("For captcha input windows to show up, you need to allow this URL to open popup windows");
	}
	
	win.onload = function() { setTimeout(function() {
			if (win.screenX === 0)
				alert("For captcha input windows to show up, you need to allow this URL to open popup windows");
		}, 0);
	};
}

function startCaptchaListener() {
	if (window.EventSource) {
		var source = new EventSource('/captcha');
		source.addEventListener('message', function (event) {
			handleCaptchaEvent(event.data);
		});
		//source.addEventListener('error', function() {
		//	window.setTimeout('startCaptchaListener()', 500);
		//});
	} else {
	}
}

function drawSpeedGraph(id, sdata) {
	var canvas = document.getElementById(id);
	var ctx = canvas.getContext("2d");
	var data = parseSpeedData(sdata);
	var seconds = graphMinutes*60;
	var width = ctx.canvas.width;
	var height = ctx.canvas.height;
	var perpt = width / Math.max(data.data.length, seconds);
	
	ctx.fillStyle = 'rgb(255,255,255)';
	ctx.fillRect(0, 0, width, height);
	
	if (!data.max) {
		ctx.save();
		ctx.font = '20px sans-serif';
		ctx.textAlign = 'center';
		ctx.textBaseline = 'middle';
		ctx.strokeStyle = 'rgb(0,0,0)';
		ctx.fillStyle = 'rgb(0,0,0)';
		ctx.fillText('NO DATA', width/2, height/2);
		ctx.restore();
		
		return;
	}
	
	data.max = Math.round(Math.max(data.max/10*11,10*1024));
	
	var pos = width;
	var elems = data.data.length;
	
	if (elems > seconds) {
		getSettingsValues(["graphminutes"], function(d) {
			graphMinutes = d["graphminutes"];
			drawSpeedGraph(id, sdata);
		});
		return;
	}
	
	// download
	ctx.strokeStyle = 'rgb(0,0,128)';
	ctx.fillStyle = 'rgba(0,0,128,0.25)';
	ctx.beginPath();
	
	for (var i = 0; i < elems; i++) {
		var y = height - height/data.max*data.data[elems - i - 1][0];
		if (i > 0)
			ctx.lineTo(pos, y);
		else
			ctx.moveTo(pos, y);
		pos -= perpt;
	}
	
	ctx.stroke();
	
	ctx.lineTo(pos+perpt, height);
	ctx.lineTo(width, height);
	ctx.fill();
	
	ctx.strokeStyle = 'rgb(128,0,0)';
	ctx.fillStyle = 'rgba(128,0,0,0.25)';
	ctx.beginPath();
	
	pos = width;
	
	for (var i = 0; i < elems; i++) {
		var y = height - height/data.max*data.data[elems - i - 1][1];
		if (i > 0)
			ctx.lineTo(pos, y);
		else
			ctx.moveTo(pos, y);
		pos -= perpt;
	}
	
	ctx.stroke();
	ctx.lineTo(pos+perpt, height);
	ctx.lineTo(width, height);
	ctx.fill();
	
	ctx.beginPath();
	ctx.strokeStyle = 'rgb(0,0,128)';
	ctx.moveTo(2,7);
	ctx.lineTo(12,7);
	ctx.stroke();
	
	ctx.beginPath();
	ctx.strokeStyle = 'rgb(128,0,0)';
	ctx.moveTo(2,19);
	ctx.lineTo(12,19);
	ctx.stroke();
	
	ctx.strokeStyle = ctx.fillStyle = 'rgb(0,0,0)';
	ctx.font = '10px sans-serif';
	ctx.fillText('Download', 15, 12);
	ctx.fillText('Upload', 15, 24);
	
	ctx.beginPath();
	for (var i = 0; i < 4; i++) {
		var x = width - (i+1)*(width/4);
		ctx.moveTo(x, height);
		ctx.lineTo(x, height-15);
		
		var mins = (seconds/4) * (i+1) / 60.0;
		mins = Math.round(mins*100)/100;
		ctx.fillText(mins+' mins ago', x+2, height-2);
	}
	ctx.stroke();
	
	ctx.strokeStyle = 'rgba(128,128,128,0.5)';
	ctx.beginPath();
	
	for (var i = 0; i < 10; i++) {
		var pos = Math.round(height / 10 * i);
		ctx.moveTo(0, pos);
		ctx.lineTo(width, pos);
		ctx.fillText(formatSize(data.max/10*(10-i))+'/s', 0, pos-10);
	}
	ctx.stroke();
}

function parseSpeedData(indata) {
	var rv = [];
	var p = indata.split(';');
	var m = 0;
	
	for (var i = 0; i < p.length; i++) {
		var v = p[i].split(',');
		
		if (!v || !v.length || !v[0].length)
			continue;
		
		rv[i] = new Array();
		rv[i][0] = parseInt(v[0]);
		rv[i][1] = parseInt(v[1]);
		
		m = Math.max(m, rv[i][0]);
		m = Math.max(m, rv[i][1]);
	}
	
	var o = {
		data: rv,
		max: m
	};
	return o;
}
