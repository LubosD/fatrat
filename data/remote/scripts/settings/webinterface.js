$.get('/fragments/settings/webinterface.html', function(data) {
	$("#settings-pane").html(data);
	
	settingsLoad();
});

function settingsLoad() {
	getSettingsValues(["remote/port", "remote/password"], function(hash) {
		$("#webinterface-port").val(hash["remote/port"]);
		$("#webinterface-password").val(hash["remote/password"]);
	});
}

function settingsSave() {
	setSettingsValue("remote/port", $("#webinterface-port").val());
	setSettingsValue("remote/password", $("#webinterface-password").val());
}
