$.get('/fragments/transfers/TorrentDownload.html', function(data) {
	$("#details-subclass").html(data);
	subclassPerformReload(getTransfer(currentTransfers[0]));
});

function subclassPerformReload(t) {
	d = new Date();
	$('#bt-progress').attr('src', '/subclass/progress?transfer='+t.uuid+'&xx='+d.getTime());
	$('#bt-availability').attr('src', '/subclass/availability?transfer='+t.uuid+'&xx='+d.getTime());
	
	client.Transfer_getAdvancedProperties(t.uuid, function(data) {
		document.getElementById('bt-download').innerHTML = data.totalDownload;
		document.getElementById('bt-upload').innerHTML = data.totalUpload;
		document.getElementById('bt-share-ratio').innerHTML = data.ratio;
		document.getElementById('bt-magnet-link').value = data.magnetLink;
		document.getElementById('bt-comment').innerHTML = data.comment;
	});
}


