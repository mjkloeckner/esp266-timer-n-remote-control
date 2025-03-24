var received_data;
var time_div, main_output_checkbox, system_local_domain, system_ip_addr, system_time, wifi_ssid, wifi_rssi;
var system_time_since_epoch;

// Convert to GMT-3 using Intl.DateTimeFormat
const time_formatter = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'America/Argentina/Buenos_Aires',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false
});

function rssi_to_percentage(rssi) {
    var quality = 0; 

    if (rssi <= -100) { 
        quality = 0; 
    } else if (rssi >= -50) { 
        quality = 100; 
    } else { 
        quality = 2 * (rssi + 100); 
    } 
    return quality; 
}

function socket_onmessage_handler(event) {
    // console.log(event.data);
    received_data = JSON.parse(event.data);

    main_output_checkbox.checked = received_data["main-output-enabled"] == "1" ? true : false;

    system_local_domain.href = "http://" + received_data["system-local-domain"] + ".local";
    system_local_domain.innerHTML = "http://" + received_data["system-local-domain"] + ".local";

    system_ip_addr.href = received_data["system-ip-addr"];
    system_ip_addr.innerHTML = received_data["system-ip-addr"];

    system_time_since_epoch = received_data["system-time"];
    const tmp_system_time = new Date(system_time_since_epoch*1000);
    const formatted_date = time_formatter.format(tmp_system_time);
    system_time.innerHTML = formatted_date + " GMT-3";

    wifi_ssid.innerHTML = received_data["wifi-ssid"];
    const rssi = received_data["wifi-rssi"];
    wifi_rssi.innerHTML = rssi + "dBm (" + rssi_to_percentage(rssi) + "%)";
}

function socket_onopen_handler(event) {
    console.log("[SOCKET] Connection established")
    socket.send("main-output-status");
}

function handle_click(cb) {
    if(cb == 'main-toggle') {
        socket.send("main-output-toggle");
    } else {
        console.log("not implemented");
    }
}

function update_system_time() {
    socket.send("main-output-status");
}

function query_data() {
    main_output_checkbox = document.getElementById("main-output-checkbox");
    system_local_domain = document.getElementById("system-local-domain");
    system_ip_addr = document.getElementById("system-ip-addr");
    system_time = document.getElementById("system-time");
    wifi_ssid = document.getElementById("wifi-ssid");
    wifi_rssi = document.getElementById("wifi-rssi");

    setInterval(update_system_time, 30*1000); // update time every 30 secs
}

const socket = new WebSocket("ws:/" + "/" + location.host + ":81");
socket.onmessage = socket_onmessage_handler;
socket.onopen = socket_onopen_handler;
window.onload = query_data;
