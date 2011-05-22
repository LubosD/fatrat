$.get('/fragments/transfers/TorrentDownload.html', function(data) {
	$("#details-subclass").html(data);
	$("#bt-magnet-link").click(function() {
		$(this).focus();
		$(this).select();
	});
	$("#torrentdownload-tabs").tabs();
	$("#torrentdownload-subclass .tabs-bottom .ui-tabs-nav, #torrentdownload-subclass .tabs-bottom .ui-tabs-nav > *")
			.removeClass( "ui-corner-all ui-corner-top" )
			.addClass( "ui-corner-bottom" );
	subclassPerformReload(getTransfer(currentTransfers[0]));
});

var scTorrentDownloadFirstLoad = true;

function subclassPerformReload(t) {
	d = new Date();
	$('#bt-progress').attr('src', '/subclass/progress?transfer='+t.uuid+'&xx='+d.getTime());
	$('#bt-availability').attr('src', '/subclass/availability?transfer='+t.uuid+'&xx='+d.getTime());
	
	var progress = singleDecimalDigit(100.0*t.done/t.total);
	var txt = '';
	var speed = t.speeds[0];
	if (t.mode.toString() == t.primaryMode.toString() && speed > 0)
		txt = ', '+formatTime( (t.total - t.done) / speed) + ' left';
	
	progress += '%';
	if (t.mode == 'Download')
		progress += ', '+formatSize(t.total-t.done)+' left';
	
	document.getElementById('bt-speed').innerHTML = formatSize(t.speeds[0])+'/s down, '+formatSize(t.speeds[1])+'/s up'+txt;
	document.getElementById('bt-progress-text').innerHTML = progress;
	
	client.Transfer_getAdvancedProperties(t.uuid, function(data) {
		document.getElementById('bt-download').innerHTML = data.totalDownload;
		document.getElementById('bt-upload').innerHTML = data.totalUpload;
		document.getElementById('bt-share-ratio').innerHTML = data.ratio;
		document.getElementById('bt-magnet-link').value = data.magnetLink;
		document.getElementById('bt-comment').innerHTML = data.comment.replace("<","&lt;").replace(">","&gt;");
		
		if (scTorrentDownloadFirstLoad) {
			for (var p=0;p<data.files.length;p++) {
				$("<tr id='torrentdownload-files-"+p+"'>"+
					"<td><input type='checkbox' rel='"+p+"' id='torrentdownload-files-"+p+"-checkbox' /><span class='filename'>"+removeTopDir(data.files[p].name)+"</span></td>"+
					"<td>"+formatSize(data.files[p].size)+"</td>"+
					"<td class='progressbar' id='torrentdownload-files-"+p+"-progress'><span></span></td>"+
					"<td class='torrentdownload-priority'><select rel='"+p+"' id='torrentdownload-files-"+p+"-select'>"+
					"<option value='0'>Do not download</option>"+
					"<option value='1'>Normal</option>"+
					"<option value='4'>Increased</option>"+
					"<option value='7'>Maximum</option>"+
					"</select></td>"+
					"</tr>").appendTo("#torrentdownload-files");
			}
			$("#torrentdownload-files input[type=checkbox]").click(function() {
				var id = $(this).attr("rel");
				var checked = $(this).is(':checked');
				$("#torrentdownload-files-"+id+"-select").val(checked ? "1" : "0").trigger('change');
			});
			$("#torrentdownload-files select").change(function() {
				var id = $(this).attr("rel");
				var val = $(this).val();
				if (val == "0")
					$("#torrentdownload-files-"+id).addClass("do-not-download");
				else
					$("#torrentdownload-files-"+id).removeClass("do-not-download");
				
				var arr = {};
				arr[parseInt(id)] = parseInt(val);
				
				client.call("TorrentDownload.setFilePriorities", t.uuid, arr, function() {});
			});
			
			scTorrentDownloadFirstLoad = false;
		}
		
		for (var p=0;p<data.files.length;p++) {
			var f = data.files[p];
			var progress = singleDecimalDigit(100.0*f.done/f.size);
			
			$("#torrentdownload-files-"+p+"-progress").progressbar({ value: progress });
			$("#torrentdownload-files-"+p+"-progress span").html(progress+"%");
			if ($("#torrentdownload-files-"+p+"-select").val() != ""+f.priority)
				$("#torrentdownload-files-"+p+"-select").val(""+f.priority);
			$("#torrentdownload-files-"+p+"-checkbox").attr("checked", f.priority > 0);
			if (f.priority == 0)
				$("#torrentdownload-files-"+p).addClass("do-not-download");
			else
				$("#torrentdownload-files-"+p).removeClass("do-not-download");
		}
	});
}

function removeTopDir(path) {
	var pos = path.indexOf("/");
	if (pos != -1)
		return "[...]"+path.substring(pos);
	else
		return path;
}


