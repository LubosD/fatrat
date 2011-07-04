;(function (w) {
  if ( !w['EventSource'] || navigator.userAgent.toLowerCase().indexOf('chrome') > -1 ) {
    // parseUri 1.2.2
    // (c) Steven Levithan <stevenlevithan.com>
    // MIT License    
    var parseUri = function(str) {
      var o   = {
          key: ['source','protocol','authority','userInfo','user','password','host','port','relative','path','directory','file','query','anchor'],
          q:   {
            name:   'queryKey',
            parser: /(?:^|&)([^&=]*)=?([^&]*)/g
          },
          parser: {
            strict: /^(?:([^:\/?#]+):)?(?:\/\/((?:(([^:@]*)(?::([^:@]*))?)?@)?([^:\/?#]*)(?::(\d*))?))?((((?:[^?#\/]*\/)*)([^?#]*))(?:\?([^#]*))?(?:#(.*))?)/
          }
        },
        m   = o.parser.strict.exec(str),
        uri = {},
        i   = 14;
      
      while (i--) uri[o.key[i]] = m[i] || '';
      
      uri[o.q.name] = {};
      uri[o.key[12]].replace(o.q.parser, function ($0, $1, $2) {
        if ($1) uri[o.q.name][$1] = $2;
      });
      
      return uri;
    };      
        
    
    //   EventSource implementation
    function EventSource( resource ) {
      
      var that        = (this === window) ? {} : this,
          retry       = 1000, offset  = 0,
          boundary    = "\n", queue   = [],   origin  = '',
          lastEventId = null, xhr     = null, source  = null, matches   = null, resourceLocation  = null;
      
      that.toString           = function () { return '[object EventSource]' };
      
      //  EventSource listener
      that.addEventListener   = function (type, listener, useCapture) {
        document.addEventListener(type, listener, useCapture);
      };
      //  EventSource dispatcher
      that.dispatchEvent      = function (event) {
        document.dispatchEvent(event);
      };
       
      resourceLocation  = parseUri(resource);
      
      //  same origin policy      
      if ( resource.match(/http/) && resource.match(/http/).length ) {
        if ( resourceLocation.host !== location.host ) {
          throw DOMException;//"SECURITY_ERR: DOM Exception";
        }
      }
      that.URL  = resourceLocation.source;
      
      var openConnectionXHR     = function() {
          
        xhr = new XMLHttpRequest();
        xhr.open('GET', that.URL, true);

        //  FIRE OPEN EVENT
        var openEvent = document.createEvent('UIEvents');
            openEvent.initEvent('open', true, true);
            openEvent.origin      = document.domain;
            openEvent.source      = null;          
            that.dispatchEvent(openEvent);     
        
        
        if ( lastEventId ) {
          xhr.setRequestHeader('Last-Event-ID', lastEventId);
        }
        xhr.onreadystatechange = function() {
          switch (xhr.readyState) {
            case 4: // disconnect case
              pseudoDispatchEvent();
              reOpenConnectionXHR();
              break;
            case 3: // new data
              processMessageFromXHR();
              break;
          }
        }
                 
        xhr.send(null);
      }      

      var reOpenConnectionXHR   = function() {
        xhr       = null;
        offset    = 0;
        setTimeout(openConnectionXHR, 1000);
      }
  
      var processMessageFromXHR = function() {
        var stream = xhr.responseText.substring(offset).split(boundary);
        
        offset = xhr.responseText.length;
        
        //  Abandon if empty
        //if ( stream.length < offset )  {
        //  return;
        //}            

        for ( var i = 0; i < stream.length; i++ ) {
          queue.push(stream[i]);
        }
        
        pseudoDispatchEvent();
      }
  
      var pseudoDispatchEvent = function() {
        var data = '', name = 'message';
        
        queue.reverse();
        
        while (queue.length > 0) {
          line = queue.pop();
          var dataIndex = line.indexOf(':'), field = null, value = '';
            
          if (dataIndex == -1) {
            field = line;
            value = '';
          }
          else if (dataIndex == 0) {
            //  Ignore Comment lines
            continue;
          }
          else {
            field = line.slice(0, dataIndex)
            value = line.slice(dataIndex+1)
          }
          
          if (field == 'event') {
            name = value;
          }
            
          if (field == 'id') {
            lastEventId = value;
          }
          
          if (field == 'retry') {
            value = parseInt(value);
            
            if (!isNaN(value)) {
              retry = value;
            }
          }
          
          if (field == 'data') {
            if (data.length > 0) {
              data += "\n";
            }
                
            data += value;
          }
        }
        
        var fireEvent = document.createEvent('UIEvents');
        //  MessageEvent causes "setting a property that has only a getter" Errors
      
        
        if ( data.length > 0 ) {
          
          fireEvent.initEvent(name, true, true);
          fireEvent.lastEventId = lastEventId;
          fireEvent.data        = data.replace(/^(\s|\u00A0)+|(\s|\u00A0)+$/g, '');
          fireEvent.origin      = document.domain;
          fireEvent.source      = null;
          that.dispatchEvent(fireEvent);
        }
      }

      //  INIT EVENT SOURCE CONNECTION
      openConnectionXHR();
    };
    window['EventSource']  = EventSource;  
  }
})(window);