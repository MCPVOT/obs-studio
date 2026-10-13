//! Minimal obs-websocket v5 client for controlling OBS streaming.
//!
//! Speaks the JSON flavour of the protocol (`obswebsocket.json`): Hello (op 0),
//! Identify (op 1), Identified (op 2), Request (op 6) and RequestResponse
//! (op 7). Events (op 5) are not subscribed to and are skipped if they arrive.
//! Protocol reference: `docs/generated/protocol.md` in obs-websocket.

#![forbid(unsafe_code)]

use std::fmt;
use std::io::{Read, Write};
use std::net::TcpStream;

use base64::Engine as _;
use base64::engine::general_purpose::STANDARD as BASE64;
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use tungstenite::client::IntoClientRequest;
use tungstenite::http::HeaderValue;
use tungstenite::{Message, WebSocket};

/// The only RPC version obs-websocket v5 defines.
pub const RPC_VERSION: u64 = 1;

/// WebSocket subprotocol for JSON-encoded messages.
pub const SUBPROTOCOL: &str = "obswebsocket.json";

/// Default obs-websocket port.
pub const DEFAULT_PORT: u16 = 4455;

const OP_HELLO: u64 = 0;
const OP_IDENTIFY: u64 = 1;
const OP_IDENTIFIED: u64 = 2;
const OP_EVENT: u64 = 5;
const OP_REQUEST: u64 = 6;
const OP_REQUEST_RESPONSE: u64 = 7;

/// Computes the Identify `authentication` string:
/// `base64(sha256(base64(sha256(password + salt)) + challenge))`.
pub fn authentication(password: &str, salt: &str, challenge: &str) -> String {
    let secret = BASE64.encode(Sha256::digest(format!("{password}{salt}")));
    BASE64.encode(Sha256::digest(format!("{secret}{challenge}")))
}

#[derive(Debug)]
pub enum Error {
    WebSocket(Box<tungstenite::Error>),
    /// The server closed the connection, e.g. code 4009 for a wrong password.
    Closed {
        code: u16,
        reason: String,
    },
    /// The server requires a password and none was given.
    PasswordRequired,
    /// A message did not follow the protocol.
    Protocol(String),
    /// The request was processed and failed (`requestStatus.result == false`).
    Request {
        code: i64,
        comment: Option<String>,
    },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::WebSocket(e) => write!(f, "websocket error: {e}"),
            Error::Closed { code, reason } if reason.is_empty() => {
                write!(f, "connection closed by server (code {code})")
            }
            Error::Closed { code, reason } => {
                write!(f, "connection closed by server (code {code}): {reason}")
            }
            Error::PasswordRequired => write!(
                f,
                "obs-websocket requires a password (use --password or OBS_WEBSOCKET_PASSWORD)"
            ),
            Error::Protocol(msg) => write!(f, "protocol error: {msg}"),
            Error::Request {
                code,
                comment: Some(comment),
            } => write!(f, "request failed (code {code}): {comment}"),
            Error::Request {
                code,
                comment: None,
            } => write!(f, "request failed (code {code})"),
        }
    }
}

impl std::error::Error for Error {}

impl From<tungstenite::Error> for Error {
    fn from(e: tungstenite::Error) -> Self {
        Error::WebSocket(Box::new(e))
    }
}

/// An identified obs-websocket session.
pub struct Client<S: Read + Write> {
    socket: WebSocket<S>,
    next_id: u64,
}

impl Client<TcpStream> {
    /// Connects to `ws://host:port` and identifies, authenticating if the
    /// server asks for it. IPv6 literals may include or omit their brackets.
    pub fn connect(host: &str, port: u16, password: Option<&str>) -> Result<Self, Error> {
        let host = host
            .strip_prefix('[')
            .and_then(|host| host.strip_suffix(']'))
            .filter(|host| host.contains(':'))
            .unwrap_or(host);
        let url = if host.contains(':') {
            format!("ws://[{host}]:{port}")
        } else {
            format!("ws://{host}:{port}")
        };
        let stream = TcpStream::connect((host, port)).map_err(tungstenite::Error::from)?;
        Self::handshake(stream, &url, password)
    }
}

impl<S: Read + Write> Client<S> {
    /// Runs the WebSocket handshake and the Hello/Identify exchange over an
    /// already-connected stream.
    pub fn handshake(stream: S, url: &str, password: Option<&str>) -> Result<Self, Error> {
        let mut request = url.into_client_request()?;
        request.headers_mut().insert(
            "Sec-WebSocket-Protocol",
            HeaderValue::from_static(SUBPROTOCOL),
        );
        let (socket, _) = tungstenite::client(request, stream).map_err(|e| match e {
            tungstenite::HandshakeError::Failure(e) => Error::from(e),
            tungstenite::HandshakeError::Interrupted(_) => {
                Error::Protocol("handshake interrupted on a non-blocking stream".into())
            }
        })?;
        let mut client = Client { socket, next_id: 0 };
        client.identify(password)?;
        Ok(client)
    }

    fn identify(&mut self, password: Option<&str>) -> Result<(), Error> {
        let hello = self.receive_op(OP_HELLO)?;
        let mut identify = json!({ "rpcVersion": RPC_VERSION, "eventSubscriptions": 0 });
        if let Some(auth) = hello.get("authentication") {
            let password = password.ok_or(Error::PasswordRequired)?;
            let field = |name: &str| {
                auth.get(name)
                    .and_then(Value::as_str)
                    .ok_or_else(|| Error::Protocol(format!("Hello authentication lacks {name}")))
            };
            identify["authentication"] =
                authentication(password, field("salt")?, field("challenge")?).into();
        }
        self.send(OP_IDENTIFY, identify)?;
        self.receive_op(OP_IDENTIFIED)?;
        Ok(())
    }

    /// Sends a request and returns its `responseData` (`Value::Null` if the
    /// response carries none).
    pub fn request(
        &mut self,
        request_type: &str,
        request_data: Option<Value>,
    ) -> Result<Value, Error> {
        self.next_id += 1;
        let request_id = self.next_id.to_string();
        let mut d = json!({ "requestType": request_type, "requestId": request_id });
        if let Some(data) = request_data {
            d["requestData"] = data;
        }
        self.send(OP_REQUEST, d)?;
        loop {
            let response = self.receive_op(OP_REQUEST_RESPONSE)?;
            if response.get("requestId").and_then(Value::as_str) != Some(&request_id) {
                continue;
            }
            let status = response
                .get("requestStatus")
                .ok_or_else(|| Error::Protocol("RequestResponse lacks requestStatus".into()))?;
            if status.get("result").and_then(Value::as_bool) != Some(true) {
                return Err(Error::Request {
                    code: status.get("code").and_then(Value::as_i64).unwrap_or(0),
                    comment: status
                        .get("comment")
                        .and_then(Value::as_str)
                        .map(str::to_owned),
                });
            }
            return Ok(response.get("responseData").cloned().unwrap_or(Value::Null));
        }
    }

    /// Closes the session cleanly.
    pub fn close(mut self) -> Result<(), Error> {
        self.socket.close(None)?;
        loop {
            match self.socket.read() {
                Ok(_) => {}
                Err(tungstenite::Error::ConnectionClosed | tungstenite::Error::AlreadyClosed) => {
                    return Ok(());
                }
                Err(e) => return Err(e.into()),
            }
        }
    }

    fn send(&mut self, op: u64, d: Value) -> Result<(), Error> {
        let text = json!({ "op": op, "d": d }).to_string();
        self.socket.send(Message::text(text))?;
        Ok(())
    }

    /// Returns the `d` of the next message with opcode `op`, skipping events.
    fn receive_op(&mut self, op: u64) -> Result<Value, Error> {
        loop {
            let text = match self.socket.read()? {
                Message::Text(text) => text,
                Message::Close(frame) => {
                    return Err(match frame {
                        Some(frame) => Error::Closed {
                            code: frame.code.into(),
                            reason: frame.reason.to_string(),
                        },
                        None => Error::Closed {
                            code: 1005,
                            reason: String::new(),
                        },
                    });
                }
                _ => continue,
            };
            let mut message: Value = serde_json::from_str(&text)
                .map_err(|e| Error::Protocol(format!("invalid JSON message: {e}")))?;
            let got = message.get("op").and_then(Value::as_u64);
            if got == Some(op) {
                return Ok(message.get_mut("d").map(Value::take).unwrap_or(Value::Null));
            }
            if got != Some(OP_EVENT) {
                return Err(Error::Protocol(format!("expected op {op}, got {text}")));
            }
        }
    }
}

/// Renders a `GetStreamStatus` response as one human-readable line.
pub fn describe_stream_status(status: &Value) -> String {
    let active = status.get("outputActive").and_then(Value::as_bool) == Some(true);
    if !active {
        return "stream: inactive".into();
    }
    let mut line = String::from("stream: live");
    if let Some(timecode) = status.get("outputTimecode").and_then(Value::as_str) {
        line.push_str(&format!(" {timecode}"));
    }
    if status.get("outputReconnecting").and_then(Value::as_bool) == Some(true) {
        line.push_str(" (reconnecting)");
    }
    if let Some(bytes) = status.get("outputBytes").and_then(Value::as_u64) {
        line.push_str(&format!(", {bytes} bytes sent"));
    }
    if let Some(skipped) = status.get("outputSkippedFrames").and_then(Value::as_u64) {
        let total = status
            .get("outputTotalFrames")
            .and_then(Value::as_u64)
            .unwrap_or(0);
        line.push_str(&format!(", {skipped}/{total} frames skipped"));
    }
    line
}
