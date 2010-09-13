parent = $('#details-subclass');
myc = document.createElement('div');
parent.append(myc);

myc.innerHTML = '<fieldset><legend>Progress</legend><img style="width: 100%; height: 30px" src="#" alt="" id="bt-progress" /</fieldset>'+
	'<fieldset><legend>Availability</legend><img style="width: 100%; height: 30px" src="#" alt="" id="bt-availability" /</fieldset>';

function subclassPerformReload(t) {
	d = new Date();
	$('#bt-progress').attr('src', '/subclass/progress?transfer='+t.uuid+'&'+d.getTime());
	$('#bt-availability').attr('src', '/subclass/availability?transfer='+t.uuid+'&'+d.getTime());
}

subclassPerformReload(getTransfer(currentTransfers[0]));
