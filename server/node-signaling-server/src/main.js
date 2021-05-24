var WebSocketServer = require('websocket').server;
var http = require('http');

var allConnections = {}

const server = http.createServer(function (request, response) {
    console.log((new Date()) + ' Received request for ' + request.url);
    response.writeHead(404);
    response.end();
});

server.listen(8555, function () {
    console.log((new Date()) + ' Server is listening on port 8555');
});

const wsServer = new WebSocketServer({
    httpServer: server,
    autoAcceptConnections: false
});

wsServer.on('request', function (request) {
    try {
        const connection = request.accept(null, request.origin);
        const uid = request.httpRequest.url.substring(1);
        allConnections[uid] = { connection, server: false, name: '', maxPlayer: 0, currentPlayer: 0, id: uid };
        console.log((new Date()) + ' Connection accepted.');
        connection.on('message', function (message) {
            if (message.type === 'utf8') {
                const data = JSON.parse(message.utf8Data)
                const { type } = data;
                if (type === 'listGameSession') {
                    let l = [];
                    for (let key in allConnections) {
                        if (allConnections[key].server) {
                            const { id, name, maxPlayer, currentPlayer, session } = allConnections[key]
                            l.push({ id: id, name, maxPlayer, currentPlayer, SessionSetting: session })
                        }
                    }
                    connection.sendUTF(JSON.stringify({ type, 'games': l }))
                } else if (type === 'createGameSession') {
                    if (allConnections[uid].server) {
                        connection.sendUTF(JSON.stringify({ type, status: false }))
                    } else {
                        allConnections[uid] = {
                            ...allConnections[uid], server: true, name: data.name, id: uid,
                            maxPlayer: data.maxPlayer || 8, currentPlayer: 1, session: data.session
                        }
                        console.log('game session created ' + uid)
                        connection.sendUTF(JSON.stringify({ type, status: true }))
                    }
                } else if (type === 'joinGameSession') {
                    const conn = allConnections[data.id]
                    if (conn) {
                        connection.sendUTF(JSON.stringify({ type, id: data.id, status: true }))
                    } else {
                        connection.sendUTF(JSON.stringify({ type, status: false }))
                    }
                } else {
                    //This is signaling message
                    const destId = data.id;
                    const destConnection = allConnections[destId];
                    if (destConnection) {
                        data.id = uid;
                        //data.remoteId =
                        destConnection.connection.sendUTF(JSON.stringify(data))
                    } else {
                        console.log('DESTINATION CLIENT NOT FOUND')
                    }
                }
            }
        });
        connection.on('close', function (reasonCode, description) {
            delete allConnections[uid];
            console.log('CONNECTION CLOSED')
        });
    } catch (e) {
        console.log("!!!!! ERROR " + e)
    }
});