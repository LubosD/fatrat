import { encodeMethodCall } from './xmlrpc-codec';
import { XmlRpcBinary } from './xmlrpc-value';

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
