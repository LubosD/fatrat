$.get('/fragments/settings/bittorrent.html', function(data) {
	$("#settings-pane").html(data);
	
	$("#bittorrent-tabs").tabs();
	
	settingsLoad();
});

function settingsLoad() {
	var keys = ["torrent/listen_start", "torrent/listen_end", "torrent/maxratio", "torrent/maxconnections",
	"torrent/maxuploads", "torrent/maxconnections_loc", "torrent/maxuploads_loc", "torrent/maxfiles",
	"torrent/dht", "torrent/pex", "torrent/allocation", "torrent/external_ip", "torrent/enc_incoming",
	"torrent/enc_outgoing", "torrent/enc_level", "torrent/enc_rc4_prefer", "torrent/mapping_upnp",
	"torrent/mapping_natpmp", "torrent/mapping_lsd"];
	
	getSettingsValues(keys, function(hash) {
		$("#bittorrent-port-start").val(hash["torrent/listen_start"]);
		$("#bittorrent-port-end").val(hash["torrent/listen_end"]);
		$("#bittorrent-seed-ratio").val(hash["torrent/maxratio"]);
		$("#bittorrent-max-connections-global").val(hash["torrent/maxconnections"]);
		$("#bittorrent-max-connections-pertorrent").val(hash["torrent/maxconnections_loc"]);
		$("#bittorrent-max-uploads-global").val(hash["torrent/maxuploads"]);
		$("#bittorrent-max-uploads-pertorrent").val(hash["torrent/maxuploads_loc"]);
		$("#bittorrent-max-open-files").val(hash["torrent/maxfiles"]);
		$("#bittorrent-allocation-mode").val(hash["torrent/allocation"]);
		$("#bittorrent-external-ip").val(hash["torrent/external_ip"]);
		$("#bittorrent-dht").attr('checked', hash["torrent/dht"] == true);
		$("#bittorrent-pex").attr('checked', hash["torrent/pex"] == true);
		$("#bittorrent-lsd").attr('checked', hash["torrent/mapping_lsd"] == true);
		$("#bittorrent-encryption-incoming").val(hash["torrent/enc_incoming"]);
		$("#bittorrent-encryption-outgoing").val(hash["torrent/enc_outgoing"]);
		$("#bittorrent-encryption-levels").val(hash["torrent/enc_level"]);
		$("#bittorrent-encryption-preferrc4").attr('checked', hash["torrent/enc_rc4_prefer"] == true);
		$("#bittorrent-portmapping-upnp").attr('checked', hash["torrent/mapping_upnp"] == true);
		$("#bittorrent-portmapping-natpmp").attr('checked', hash["torrent/mapping_natpmp"] == true);
	});
}

function checkInt(varid) {
	el = $('#'+varid);
	v = parseInt(el.val());
	
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

function checkFloat(val) {
	v = parseFloat(val);
	if (isNaN(v))
		throw "String entered instead of a number";
	return v;
}

function b2s(val) {
	return (val)? "true" : "false";
}

function settingsSave() {
	port_start = checkInt("bittorrent-port-start");
	port_end = checkInt("bittorrent-port-end");
	if (port_start < 1024 || port_end < 1024 || port_end < port_start)
		throw "Invalid port range specified";
	
	setSettingsValue("torrent/listen_start", port_start);
	setSettingsValue("torrent/listen_end", port_end);
	setSettingsValue("torrent/seed_ratio", checkFloat($("#bittorrent-seed-ratio").val()));
	setSettingsValue("torrent/maxconnections", checkInt("bittorrent-max-connections-global"));
	setSettingsValue("torrent/maxconnections_loc", checkInt("bittorrent-max-connections-pertorrent"));
	setSettingsValue("torrent/maxuploads", checkInt("bittorrent-max-uploads-global"));
	setSettingsValue("torrent/maxuploads_loc", checkInt("bittorrent-max-uploads-pertorrent"));
	setSettingsValue("torrent/maxfiles", checkInt("bittorrent-max-open-files"));
	
	setSettingsValue("torrent/dht", $("#bittorrent-dht").is(":checked"));
	setSettingsValue("torrent/pex", $("#bittorrent-pex").is(":checked"));
	setSettingsValue("torrent/mapping_lsd", $("#bittorrent-lsd").is(":checked"));
	setSettingsValue("torrent/mapping_upnp", $("#bittorrent-portmapping-upnp").is(":checked"));
	setSettingsValue("torrent/mapping_natpmp", $("#bittorrent-portmapping-natpmp").is(":checked"));
	setSettingsValue("torrent/enc_rc4_prefer", $("#bittorrent-encryption-preferrc4").is(":checked"));
	setSettingsValue("torrent/enc_incoming", $("#bittorrent-encryption-incoming").val());
	setSettingsValue("torrent/enc_outgoing", $("#bittorrent-encryption-outgoing").val());
	setSettingsValue("torrent/enc_level", $("#bittorrent-encryption-levels").val());
	setSettingsValue("torrent/external_ip", $("#bittorrent-external-ip").val());
	setSettingsValue("torrent/allocation", $("#bittorrent-allocation-mode").val());
}
