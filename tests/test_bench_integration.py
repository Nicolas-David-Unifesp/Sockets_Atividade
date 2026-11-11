"""CA6: barreira GO, soma exata de payload e encerramento/CSV."""
import socket
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    server = subprocess.Popen([str(ROOT / "server"), str(port), "--bench", "2"],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    connections = []
    try:
        assert "aguardando" in server.stderr.readline()
        first = socket.create_connection(("127.0.0.1", port), timeout=3)
        connections.append(first)
        first.sendall(b"primeiro\n")
        first.settimeout(0.2)
        try:
            first.recv(3)
            raise AssertionError("GO antes de todos os clientes conectarem")
        except socket.timeout:
            pass
        second = socket.create_connection(("127.0.0.1", port), timeout=3)
        connections.append(second)
        second.sendall(b"segundo\n")
        for conn in connections:
            conn.settimeout(3)
            line = b""
            while not line.endswith(b"\n"):
                part = conn.recv(1)
                assert part
                line += part
            assert line == b"GO\n"
        for conn, count in zip(connections, [12345, 67890]):
            conn.sendall(b"x" * count)
            conn.shutdown(socket.SHUT_WR)
        stdout, stderr = server.communicate(timeout=5)
        assert server.returncode == 0, stderr
        assert len(stdout.splitlines()) == 1
        n, total, seconds, mbps = stdout.strip().split(",")
        assert int(n) == 2 and int(total) == 80235
        assert float(seconds) >= 0 and float(mbps) > 0
        print("CA6: barreira GO, 80235 bytes agregados, CSV único e encerramento: PASSOU")
    finally:
        for conn in connections:
            conn.close()
        if server.poll() is None:
            server.kill()
        server.wait()

if __name__ == "__main__":
    main()
