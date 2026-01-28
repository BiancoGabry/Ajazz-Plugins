using NAudio.CoreAudioApi;
using System.Diagnostics;
using System.Linq;

namespace MultiVolumeBackend
{
    // Creiamo un "pacchetto" per trasportare sia il volume che lo stato Mute
    public struct VolumeData
    {
        public float Volume;
        public bool IsMuted;
    }

    public class AudioHelper
    {
        // Nota: Ora restituisce "VolumeData", non più "float"
        public static VolumeData GetVolumeForProcess(string rawInput)
        {
            // Valore di default (Volume 0, non mutato)
            VolumeData result = new VolumeData { Volume = 0, IsMuted = false };

            if (string.IsNullOrEmpty(rawInput)) return result;

            var candidates = rawInput.Split(',')
                                     .Select(p => p.ToLower().Replace(".exe", "").Trim())
                                     .Where(p => !string.IsNullOrEmpty(p))
                                     .ToList();

            if (candidates.Count == 0) return result;

            var deviceEnumerator = new MMDeviceEnumerator();
            var device = deviceEnumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);

            for (int i = 0; i < device.AudioSessionManager.Sessions.Count; i++)
            {
                var session = device.AudioSessionManager.Sessions[i];
                uint pid = session.GetProcessID;
                if (pid == 0) continue;

                try
                {
                    var p = Process.GetProcessById((int)pid);
                    if (candidates.Contains(p.ProcessName.ToLower()))
                    {
                        if (session.SimpleAudioVolume != null)
                        {
                            // RIEMPIAMO IL PACCHETTO DATI
                            result.Volume = session.SimpleAudioVolume.Volume * 100;
                            result.IsMuted = session.SimpleAudioVolume.Mute; // Legge se è muto
                            return result;
                        }
                    }
                }
                catch { }
            }

            return result;
        }
    }
}