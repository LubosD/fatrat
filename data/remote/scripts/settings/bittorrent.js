$.get('/fragments/settings/bittorrent.html', function(data) {
	$("#settings-pane").html(data);
	
	$("#bittorrent-tabs").tabs();
	
	settingsLoad();
});

function settingsLoad() {
	/*getSettingsValues(["remote/port", "remote/password"], function(hash) {
		$("#webinterface-port").val(hash["remote/port"]);
		$("#webinterface-password").val(hash["remote/password"]);
	});*/
}

function settingsSave() {
	
}
