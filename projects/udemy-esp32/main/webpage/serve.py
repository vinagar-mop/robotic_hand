import http.server, socketserver, os
os.chdir('/Users/VinayAgarwal/Downloads/robotic_hand-master/projects/udemy-esp32/main/webpage')
PORT = 7788
with socketserver.TCPServer(("", PORT), http.server.SimpleHTTPRequestHandler) as h:
    h.serve_forever()
