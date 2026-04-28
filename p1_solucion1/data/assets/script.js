// Intervalo de actualización en milisegundos (5000 = 5 segundos)
const INTERVALO_MS = 5000;

function actualizarSensores() {
  fetch("/data")
    .then(response => response.json())
    .then(data => {
      document.getElementById("temp").textContent = data.temp;
      document.getElementById("hum").textContent  = data.hum;
      document.getElementById("led-status").textContent = data.led ? "ENCENDIDO" : "APAGADO";
    })
    .catch(err => console.error("Error al obtener datos:", err));
}

function setLed(state) {
  fetch("/update?led_state=" + state)
    .then(response => response.json())
    .then(data => {
      document.getElementById("led-status").textContent = data.led ? "ENCENDIDO" : "APAGADO";
    });
}

actualizarSensores();
setInterval(actualizarSensores, INTERVALO_MS);
