$.get('/fragments/transfers/TorrentDownload.html', function(data) {
	$("#details-subclass").html(data);
	$("#bt-magnet-link").click(function() {
		$(this).focus();
		$(this).select();
	});
	subclassPerformReload(getTransfer(currentTransfers[0]));
});

function subclassPerformReload(t) {
	d = new Date();
	$('#bt-progress').attr('src', '/subclass/progress?transfer='+t.uuid+'&xx='+d.getTime());
	$('#bt-availability').attr('src', '/subclass/availability?transfer='+t.uuid+'&xx='+d.getTime());
	
	var progress = singleDecimalDigit(100.0*t.done/t.total);
	var txt = '';
	var speed = t.speeds[0];
	if (t.mode.toString() == t.primaryMode.toString() && speed > 0)
		txt = ', '+formatTime( (t.total - t.done) / speed );
	
	progress += '%, '+formatSize(t.total-t.done)+' left';
	
	document.getElementById('bt-speed').innerHTML = formatSize(t.speeds[0])+'/s down, '+formatSize(t.speeds[1])+'/s up'+txt;
	document.getElementById('bt-progress-text').innerHTML = progress;
	
	client.Transfer_getAdvancedProperties(t.uuid, function(data) {
		document.getElementById('bt-download').innerHTML = data.totalDownload;
		document.getElementById('bt-upload').innerHTML = data.totalUpload;
		document.getElementById('bt-share-ratio').innerHTML = data.ratio;
		document.getElementById('bt-magnet-link').value = data.magnetLink;
		document.getElementById('bt-comment').innerHTML = data.comment;
	});
}


