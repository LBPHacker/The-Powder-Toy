from OpenSSL import crypto
import http.server
import json
import os
import re
import ssl
import sys

HTTP_HOST         = '127.0.0.1'
HTTP_PORT         = 8000
KEY_FILE          = 'key.pem'
CERT_FILE         = 'cert.crt'
CERT_CN           = 'commonName'
CERT_EMAILADDRESS = 'emailAddress'
CERT_C            = 'NT' # country
CERT_ST           = 'stateOrProvinceName'
CERT_L            = 'localityName'
CERT_O            = 'organizationName'
CERT_OU           = 'organizationUnitName'
CERT_SN           = 0 # serialNumber
CERT_TTL          = 30 * 24 * 60 * 60

print(f'generating index for @APP_EXE@.js')
with open('index.html', 'wt') as f:
	f.write(f'''
<!DOCTYPE html>
<html lang="en">
	<body>
		<p>Remember, this is being served locally, so it will not be able to connect to <a href="@SERVER@">@SERVER@</a>. To enable this, serve the following files there:</p>
		<ul>
			<li>@APP_EXE@.js</li>
			<li>@APP_EXE@.wasm</li>
			<li>@APP_EXE@.wasm.map for debugging purposes</li>
		</ul>
		<p id="status" style="display: none;">Loading...</p>
		<canvas style="display: none;" class="emscripten" id="canvas" oncontextmenu="event.preventDefault()" tabindex=-1></canvas>
		<script type='text/javascript'>
			(() => {{
				var promise;

				window.create_@APP_EXE_JSSAFE@_loader = () => {{
					if (promise === undefined) {{
						promise = new Promise((resolve, reject) => {{
							const script = document.createElement('script');
							script.onload = () => {{
								resolve(window.create_@APP_EXE_JSSAFE@);
							}};
							document.head.appendChild(script);
							script.src = '@APP_EXE@.js';
						}});
					}}
					return promise;
				}};
			}})();
			(() => {{
				var canvas = document.getElementById('canvas');
				var status = document.getElementById('status');
				window.mark_presentable = function() {{
					canvas.style.display = 'initial';
				}};
				window.onerror = (event) => {{
					status.innerText = 'Exception thrown, see JavaScript console';
					status.style.display = 'initial';
				}};
				create_@APP_EXE_JSSAFE@_loader().then(create_@APP_EXE_JSSAFE@ => create_@APP_EXE_JSSAFE@({{
					canvas: (() => {{
						canvas.addEventListener('webglcontextlost', e => {{
							alert('WebGL context lost. You will need to reload the page.'); e.preventDefault();
						}}, false);
						return canvas;
					}})(),
					print: console.log,
					printErr: console.log,
				}}));
			}})();
		</script>
	</body>
</html>
''')

print('generating keypair')
key = crypto.PKey()
key.generate_key(crypto.TYPE_RSA, 2048)
cert = crypto.X509()
cert.get_subject().CN = CERT_CN
cert.get_subject().emailAddress = CERT_EMAILADDRESS
cert.get_subject().C = CERT_C
cert.get_subject().ST = CERT_ST
cert.get_subject().L = CERT_L
cert.get_subject().O = CERT_O
cert.get_subject().OU = CERT_OU
cert.set_serial_number(CERT_SN)
cert.gmtime_adj_notBefore(0)
cert.gmtime_adj_notAfter(CERT_TTL)
cert.set_issuer(cert.get_subject())
cert.set_pubkey(key)
cert.sign(key, 'sha256')
with open(CERT_FILE, 'wt') as f:
	f.write(crypto.dump_certificate(crypto.FILETYPE_PEM, cert).decode('utf-8'))
with open(KEY_FILE, 'wt') as f:
	f.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, key).decode('utf-8'))

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
	def end_headers(self):
		self.send_my_headers()
		http.server.SimpleHTTPRequestHandler.end_headers(self)

	def send_my_headers(self):
		self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
		self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')

server_address = (HTTP_HOST, HTTP_PORT)
httpd = http.server.HTTPServer(server_address, MyHTTPRequestHandler)
ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
print(f'serving at https://{HTTP_HOST}:{HTTP_PORT}, Ctrl+C to exit')
httpd.serve_forever()
