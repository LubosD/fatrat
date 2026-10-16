import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { XmlRpcTransferApi } from './xmlrpc-transfer-api';
import { XmlRpcClient } from '../xmlrpc/xmlrpc-client';

describe('XmlRpcTransferApi', () => {
  let api: XmlRpcTransferApi;
  let client: XmlRpcClient;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting(), XmlRpcClient, XmlRpcTransferApi],
    });
    api = TestBed.inject(XmlRpcTransferApi);
    client = TestBed.inject(XmlRpcClient);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('getQueues() calls the getQueues method and maps struct fields', () => {
    return new Promise<void>((done) => {
      api.getQueues().subscribe((queues) => {
        expect(queues).toEqual([
          {
            uuid: 'q1',
            name: 'Default',
            defaultDirectory: '/downloads',
            moveDirectory: '',
            upAsDown: false,
            transferLimits: { down: 0, up: 0 },
            speedLimits: { down: 0, up: 0 },
          },
        ]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>getQueues</methodName>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data><value><struct>' +
          '<member><name>name</name><value><string>Default</string></value></member>' +
          '<member><name>uuid</name><value><string>q1</string></value></member>' +
          '<member><name>defaultDirectory</name><value><string>/downloads</string></value></member>' +
          '<member><name>moveDirectory</name><value><string></string></value></member>' +
          '<member><name>upAsDown</name><value><boolean>0</boolean></value></member>' +
          '<member><name>transferLimits</name><value><array><data><value><int>0</int></value><value><int>0</int></value></data></array></value></member>' +
          '<member><name>speedLimits</name><value><array><data><value><int>0</int></value><value><int>0</int></value></data></array></value></member>' +
          '</struct></value></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('getQueueTransfers() calls Queue.getTransfers with the queue uuid', () => {
    return new Promise<void>((done) => {
      api.getQueueTransfers('q1').subscribe((transfers) => {
        expect(transfers).toEqual([]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.getTransfers</methodName>');
      expect(req.request.body).toContain('<value><string>q1</string></value>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('deleteTransfers() calls Transfer.delete with uuids and withData', () => {
    return new Promise<void>((done) => {
      api.deleteTransfers(['t1', 't2'], true).subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Transfer.delete</methodName>');
      expect(req.request.body).toContain('<value><string>t1</string></value>');
      expect(req.request.body).toContain('<value><string>t2</string></value>');
      expect(req.request.body).toContain('<value><boolean>1</boolean></value>');
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });

  it('moveTransfers() calls Queue.moveTransfers with the direction string', () => {
    return new Promise<void>((done) => {
      api.moveTransfers('q1', ['t1'], 'top').subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.moveTransfers</methodName>');
      expect(req.request.body).toContain('<string>top</string>');
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });

  it('getTransferSpeedGraph() parses the "down,up;down,up;" string format', () => {
    return new Promise<void>((done) => {
      api.getTransferSpeedGraph('t1').subscribe((samples) => {
        expect(samples).toEqual([
          { down: 12, up: 3 },
          { down: 15, up: 4 },
        ]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Transfer.getSpeedData</methodName>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><string>12,3;15,4;</string></value></param></params></methodResponse>',
      );
    });
  });

  it('getTransferSpeedGraph() returns an empty array for an empty string', () => {
    return new Promise<void>((done) => {
      api.getTransferSpeedGraph('t1').subscribe((samples) => {
        expect(samples).toEqual([]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><string></string></value></param></params></methodResponse>',
      );
    });
  });

  it('addTransfers() calls Queue.addTransfers with positional args in the exact backend order', () => {
    return new Promise<void>((done) => {
      api
        .addTransfers(['http://example.com/a'], {
          upload: false,
          queueUuid: 'q1',
          targetClass: '',
          target: '/downloads',
          paused: false,
          speedLimits: { down: 0, up: 0 },
        })
        .subscribe((uuids) => {
          expect(uuids).toEqual(['t1']);
          done();
        });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.addTransfers</methodName>');
      // bool upload, QString uuid, QStringList urls, QString class, QString target, bool paused, int down, int up
      expect(req.request.body).toBe(
        '<?xml version="1.0"?><methodCall><methodName>Queue.addTransfers</methodName><params>' +
          '<param><value><boolean>0</boolean></value></param>' +
          '<param><value><string>q1</string></value></param>' +
          '<param><value><array><data><value><string>http://example.com/a</string></value></data></array></value></param>' +
          '<param><value><string></string></value></param>' +
          '<param><value><string>/downloads</string></value></param>' +
          '<param><value><boolean>0</boolean></value></param>' +
          '<param><value><int>0</int></value></param>' +
          '<param><value><int>0</int></value></param>' +
          '</params></methodCall>',
      );
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data><value><string>t1</string></value></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('getSettingsPages() maps struct fields', () => {
    return new Promise<void>((done) => {
      api.getSettingsPages().subscribe((pages) => {
        expect(pages).toEqual([{ title: 'BitTorrent', script: 'bittorrent.js', icon: 'bt.png' }]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Settings.getPages</methodName>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data><value><struct>' +
          '<member><name>title</name><value><string>BitTorrent</string></value></member>' +
          '<member><name>script</name><value><string>bittorrent.js</string></value></member>' +
          '<member><name>icon</name><value><string>bt.png</string></value></member>' +
          '</struct></value></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('addTransferWithData() never calls the XML-RPC client when unsubscribed before the file finishes reading', async () => {
    const file = new File(['hello'], 'hello.txt');
    const callSpy = vi.spyOn(client, 'call');

    const sub = api
      .addTransferWithData(file, {
        upload: false,
        queueUuid: 'q1',
        targetClass: '',
        target: '/downloads',
        paused: false,
        speedLimits: { down: 0, up: 0 },
      })
      .subscribe();
    sub.unsubscribe();

    // Give the FileReader's async read a chance to actually complete before asserting nothing fired.
    await new Promise((resolve) => setTimeout(resolve, 50));

    // The RxJS Subscriber this Observable forwards to XmlRpcClient.call() is already closed by
    // the time FileReader resolves, so RxJS's own isStopped guard silently drops the eventual
    // HTTP request no matter what - httpMock can't tell the fixed code apart from the buggy one.
    // What genuinely distinguishes them is whether we bother calling into the client at all once
    // cancelled: the buggy code called it unconditionally from the FileReader callback; the fixed
    // code checks a `cancelled` flag first and skips the call entirely.
    expect(callSpy).not.toHaveBeenCalled();
    httpMock.expectNone('/xmlrpc');
  });

  it('addTransferWithData() sends the request normally when not unsubscribed', async () => {
    const file = new File(['hello'], 'hello.txt');

    const resultPromise = new Promise<string>((resolve) => {
      api
        .addTransferWithData(file, {
          upload: false,
          queueUuid: 'q1',
          targetClass: '',
          target: '/downloads',
          paused: false,
          speedLimits: { down: 0, up: 0 },
        })
        .subscribe(resolve);
    });

    // Give the FileReader's async read a chance to complete so the HTTP call actually fires.
    await new Promise((resolve) => setTimeout(resolve, 50));

    const req = httpMock.expectOne('/xmlrpc');
    expect(req.request.body).toContain('<methodName>Queue.addTransferWithData</methodName>');
    req.flush(
      '<?xml version="1.0"?><methodResponse><params><param><value><string>t1</string></value></param></params></methodResponse>',
    );

    expect(await resultPromise).toEqual('t1');
  });

  it('createQueue() array-encodes speedLimits instead of struct-encoding it', () => {
    return new Promise<void>((done) => {
      api.createQueue({ name: 'Movies', speedLimits: { down: 100, up: 50 } }).subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.create</methodName>');
      expect(req.request.body).toContain(
        '<member><name>speedLimits</name><value><array><data><value><int>100</int></value><value><int>50</int></value></data></array></value></member>',
      );
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });

  it('setQueueProperties() array-encodes transferLimits instead of struct-encoding it', () => {
    return new Promise<void>((done) => {
      api
        .setQueueProperties('q1', { transferLimits: { down: 5, up: 3 } })
        .subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.setProperties</methodName>');
      expect(req.request.body).toContain(
        '<member><name>transferLimits</name><value><array><data><value><int>5</int></value><value><int>3</int></value></data></array></value></member>',
      );
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });

  it('createQueue() without speedLimits/transferLimits only sends the given fields', () => {
    return new Promise<void>((done) => {
      api.createQueue({ name: 'X' }).subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<methodName>Queue.create</methodName>');
      expect(req.request.body).toContain(
        '<param><value><struct><member><name>name</name><value><string>X</string></value></member></struct></value></param>',
      );
      expect(req.request.body).not.toContain('speedLimits');
      expect(req.request.body).not.toContain('transferLimits');
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });

  it('callRaw() forwards the method name and params verbatim', () => {
    return new Promise<void>((done) => {
      api.callRaw('TorrentDownload.setFilePriorities', 't1', { '0': 1 }).subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain(
        '<methodName>TorrentDownload.setFilePriorities</methodName>',
      );
      req.flush('<?xml version="1.0"?><methodResponse><params></params></methodResponse>');
    });
  });
});
