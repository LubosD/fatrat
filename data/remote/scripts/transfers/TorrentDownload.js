parent = $('#details-subclass');
myc = document.createElement('div');
parent.append(myc);

myc.innerHTML = '<fieldset><legend>Progress</legend><img style="width: 100%; height: 30px" alt="" id="bt-progress" /</fieldset>'+
	'<fieldset><legend>Availability</legend><img style="width: 100%; height: 30px" alt="" id="bt-availability" /</fieldset>'+
	'<table style="width:100%"><tr><td>Total download:</td><td id="bt-download" style="width:40%"></td><td>Total upload:</td><td id="bt-upload" style="width:40%"></td></tr>'+
	'<tr><td>Share ratio:</td><td id="bt-share-ratio"></td><td>Magnet link:</td><td><input type="text" style="width:100%" readonly="readonly" id="bt-magnet-link" /></td></tr>'+
	'<tr><td>Comment:</td><td colspan="3" id="bt-comment" style="border: 1px solid #777; padding: 1ex"></td></tr></table>';

function subclassPerformReload(t) {
	d = new Date();
	$('#bt-progress').attr('src', '/subclass/progress?transfer='+t.uuid+'&'+d.getTime());
	$('#bt-availability').attr('src', '/subclass/availability?transfer='+t.uuid+'&'+d.getTime());
	
	client.Transfer_getAdvancedProperties(t.uuid, function(data) {
		document.getElementById('bt-download').innerHTML = data.totalDownload;
		document.getElementById('bt-upload').innerHTML = data.totalUpload;
		document.getElementById('bt-share-ratio').innerHTML = data.ratio;
		document.getElementById('bt-magnet-link').value = data.magnetLink;
		document.getElementById('bt-comment').innerHTML = data.comment;
	});
}

subclassPerformReload(getTransfer(currentTransfers[0]));
