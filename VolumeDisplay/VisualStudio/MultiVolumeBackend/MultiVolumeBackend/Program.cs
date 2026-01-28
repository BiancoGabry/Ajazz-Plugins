using System;
using System.Collections.Generic;
using System.IO;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace MultiVolumeBackend
{
    internal class Program
    {
        private static ClientWebSocket _socket = new ClientWebSocket();
        private static string _pluginUUID = "";
        private static string _registerEvent = "";
        private static int _port = 0;
        private static HashSet<string> _activeContexts = new HashSet<string>();

        // Questa lista contiene le app configurate dall'utente
        private static List<AppConfig> _currentApps = new List<AppConfig>();

        static async Task Main(string[] args)
        {
            // Parsing argomenti
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "-port" && i + 1 < args.Length) _port = int.Parse(args[i + 1]);
                if (args[i] == "-pluginUUID" && i + 1 < args.Length) _pluginUUID = args[i + 1];
                if (args[i] == "-registerEvent" && i + 1 < args.Length) _registerEvent = args[i + 1];
            }

            if (_port == 0) return;

            try
            {
                await _socket.ConnectAsync(new Uri($"ws://127.0.0.1:{_port}"), CancellationToken.None);

                var regJson = new { @event = _registerEvent, uuid = _pluginUUID };
                await SendJson(regJson);

                _ = Task.Run(() => UpdateLoop());
                await ReceiveLoop();
            }
            catch { /* Gestione errori silenziosa o su file */ }
        }

        private static async Task UpdateLoop()
        {
            while (_socket.State == WebSocketState.Open)
            {
                if (_activeContexts.Count > 0)
                {
                    try
                    {
                        // Genera l'immagine REALE basata sulla lista _currentApps
                        string base64 = ImageDrawer.DrawImage(_currentApps);

                        foreach (var context in _activeContexts)
                        {
                            var payload = new
                            {
                                @event = "setImage",
                                context = context,
                                payload = new { image = base64, target = 0 }
                            };
                            await SendJson(payload);
                        }
                    }
                    catch { }
                }
                await Task.Delay(150); // Aggiornamento fluido
            }
        }

        private static async Task ReceiveLoop()
        {
            var buffer = new byte[65536];
            while (_socket.State == WebSocketState.Open)
            {
                try
                {
                    var result = await _socket.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);
                    if (result.MessageType == WebSocketMessageType.Close) break;

                    string jsonString = Encoding.UTF8.GetString(buffer, 0, result.Count);
                    HandleMessage(jsonString);
                }
                catch { }
            }
        }

        private static void HandleMessage(string json)
        {
            var msg = JObject.Parse(json);
            string eventName = msg["event"]?.ToString();
            string context = msg["context"]?.ToString();

            if (eventName == "willAppear")
            {
                if (!string.IsNullOrEmpty(context)) _activeContexts.Add(context);

                // Se c'erano impostazioni salvate, caricale subito!
                if (msg["payload"]?["settings"] != null)
                {
                    ParseSettings(msg["payload"]["settings"]);
                }
            }
            else if (eventName == "willDisappear")
            {
                if (!string.IsNullOrEmpty(context)) _activeContexts.Remove(context);
            }
            else if (eventName == "didReceiveSettings")
            {
                // L'utente ha modificato qualcosa nel software Ajazz
                if (msg["payload"]?["settings"] != null)
                {
                    ParseSettings(msg["payload"]["settings"]);
                }
            }
        }

        private static void ParseSettings(JToken settings)
        {
            try
            {
                var appsToken = settings["apps"];
                if (appsToken != null)
                {
                    var newApps = appsToken.ToObject<List<AppConfig>>();
                    if (newApps != null)
                    {
                        _currentApps = newApps;
                    }
                }
            }
            catch { }
        }

        private static async Task SendJson(object data)
        {
            string json = JsonConvert.SerializeObject(data);
            byte[] bytes = Encoding.UTF8.GetBytes(json);
            await _socket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, CancellationToken.None);
        }
    }
}