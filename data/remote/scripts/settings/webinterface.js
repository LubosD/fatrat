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
	port = parseInt($("#webinterface-port").val());
	if (isNaN(port) || port < 1024 || port > 65535)
		throw "Invalid port specified";
	
	setSettingsValue("remote/port", port);
	setSettingsValue("remote/password", $("#webinterface-password").val());
}
