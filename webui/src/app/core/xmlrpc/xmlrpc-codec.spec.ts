import { decodeMethodResponse, encodeMethodCall } from './xmlrpc-codec';
import { XmlRpcBinary, XmlRpcFault } from './xmlrpc-value';

describe('encodeMethodCall', () => {
  it('encodes a call with no params', () => {
    const xml = encodeMethodCall('getQueues', []);
    expect(xml).toBe('<?xml version="1.0"?><methodCall><methodName>getQueues</methodName></methodCall>');
  });

  it('encodes string params, escaping XML-sensitive characters', () => {
    const xml = encodeMethodCall('echo', ['a & b <c> "d"']);
    expect(xml).toContain('<string>a &amp; b &lt;c&gt; "d"</string>');
  });

  it('encodes integers as <int> and non-integers as <double>', () => {
    const xml = encodeMethodCall('echo', [42, 3.5]);
    expect(xml).toContain('<value><int>42</int></value>');
    expect(xml).toContain('<value><double>3.5</double></value>');
  });

  it('encodes booleans as 1/0', () => {
    const xml = encodeMethodCall('echo', [true, false]);
    expect(xml).toContain('<value><boolean>1</boolean></value>');
    expect(xml).toContain('<value><boolean>0</boolean></value>');
  });

  it('encodes arrays', () => {
    const xml = encodeMethodCall('echo', [['a', 'b']]);
    expect(xml).toContain(
      '<array><data><value><string>a</string></value><value><string>b</string></value></data></array>',
    );
  });

  it('encodes plain objects as structs', () => {
    const xml = encodeMethodCall('echo', [{ name: 'q1', upAsDown: true }]);
    expect(xml).toContain(
      '<struct><member><name>name</name><value><string>q1</string></value></member>' +
        '<member><name>upAsDown</name><value><boolean>1</boolean></value></member></struct>',
    );
  });

  it('encodes dates as dateTime.iso8601', () => {
    const xml = encodeMethodCall('echo', [new Date(2026, 0, 5, 9, 3, 7)]);
    expect(xml).toContain('<dateTime.iso8601>20260105T9:3:7</dateTime.iso8601>');
  });

  it('encodes XmlRpcBinary as base64', () => {
    const xml = encodeMethodCall('echo', [XmlRpcBinary.fromBase64('AAECAw==')]);
    expect(xml).toContain('<value><base64>AAECAw==</base64></value>');
  });

  it('wraps multiple params correctly', () => {
    const xml = encodeMethodCall('Queue_getTransfers', ['uuid-1']);
    expect(xml).toBe(
      '<?xml version="1.0"?><methodCall><methodName>Queue_getTransfers</methodName>' +
        '<params><param><value><string>uuid-1</string></value></param></params></methodCall>',
    );
  });
});

describe('decodeMethodResponse', () => {
  function response(paramsInner: string): string {
    return `<?xml version="1.0"?><methodResponse><params>${paramsInner}</params></methodResponse>`;
  }

  it('decodes a single string param', () => {
    const xml = response('<param><value><string>hello</string></value></param>');
    expect(decodeMethodResponse(xml)).toBe('hello');
  });

  it('decodes int and double', () => {
    expect(decodeMethodResponse(response('<param><value><int>7</int></value></param>'))).toBe(7);
    expect(decodeMethodResponse(response('<param><value><double>1.5</double></value></param>'))).toBe(1.5);
  });

  it('decodes boolean', () => {
    expect(decodeMethodResponse(response('<param><value><boolean>1</boolean></value></param>'))).toBe(true);
    expect(decodeMethodResponse(response('<param><value><boolean>0</boolean></value></param>'))).toBe(false);
  });

  it('decodes dateTime.iso8601', () => {
    const xml = response('<param><value><dateTime.iso8601>20260105T09:03:07</dateTime.iso8601></value></param>');
    const result = decodeMethodResponse(xml) as Date;
    expect(result.getFullYear()).toBe(2026);
    expect(result.getMonth()).toBe(0);
    expect(result.getDate()).toBe(5);
    expect(result.getHours()).toBe(9);
    expect(result.getMinutes()).toBe(3);
    expect(result.getSeconds()).toBe(7);
  });

  it('decodes arrays', () => {
    const xml = response(
      '<param><value><array><data>' +
        '<value><string>a</string></value><value><string>b</string></value>' +
        '</data></array></value></param>',
    );
    expect(decodeMethodResponse(xml)).toEqual(['a', 'b']);
  });

  it('decodes structs', () => {
    const xml = response(
      '<param><value><struct>' +
        '<member><name>name</name><value><string>q1</string></value></member>' +
        '<member><name>upAsDown</name><value><boolean>1</boolean></value></member>' +
        '</struct></value></param>',
    );
    expect(decodeMethodResponse(xml)).toEqual({ name: 'q1', upAsDown: true });
  });

  it('decodes base64', () => {
    const xml = response('<param><value><base64>AAECAw==</base64></value></param>');
    expect(decodeMethodResponse(xml)).toEqual({ base64: 'AAECAw==' });
  });

  it('returns an array of values for multiple params', () => {
    const xml = response(
      '<param><value><int>1</int></value></param><param><value><int>2</int></value></param>',
    );
    expect(decodeMethodResponse(xml)).toEqual([1, 2]);
  });

  it('returns null for an empty params list', () => {
    expect(decodeMethodResponse(response(''))).toBeNull();
  });

  it('throws XmlRpcFault on a fault response', () => {
    const xml =
      '<?xml version="1.0"?><methodResponse><fault><value><struct>' +
      '<member><name>faultCode</name><value><int>101</int></value></member>' +
      '<member><name>faultString</name><value><string>Invalid queue UUID</string></value></member>' +
      '</struct></value></fault></methodResponse>';
    let caught: unknown;
    try {
      decodeMethodResponse(xml);
    } catch (e) {
      caught = e;
    }
    expect(caught).toBeInstanceOf(XmlRpcFault);
    expect((caught as XmlRpcFault).faultCode).toBe(101);
    expect((caught as XmlRpcFault).faultString).toBe('Invalid queue UUID');
  });

  it('throws when the response has no methodResponse root', () => {
    const xml = '<?xml version="1.0"?><html><body>502 Bad Gateway</body></html>';
    expect(() => decodeMethodResponse(xml)).toThrow(/methodResponse/);
  });

  it('throws when a struct member is missing name or value', () => {
    const xml = response(
      '<param><value><struct>' + '<member><name>name</name></member>' + '</struct></value></param>',
    );
    expect(() => decodeMethodResponse(xml)).toThrow(/member/);
  });

  it('throws when the fault struct is missing faultCode or faultString', () => {
    const xml =
      '<?xml version="1.0"?><methodResponse><fault><value><struct>' +
      '<member><name>faultString</name><value><string>oops</string></value></member>' +
      '</struct></value></fault></methodResponse>';
    expect(() => decodeMethodResponse(xml)).toThrow(/faultCode|faultString/);
  });
});
