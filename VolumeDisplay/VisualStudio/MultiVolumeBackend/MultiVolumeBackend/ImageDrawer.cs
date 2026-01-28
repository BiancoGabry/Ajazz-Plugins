using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;

namespace MultiVolumeBackend
{
    public class AppConfig
    {
        public string ExeName { get; set; }
        public string Label { get; set; }
        public string HexColor { get; set; }
    }

    public class ImageDrawer
    {
        public static string DrawImage(List<AppConfig> apps)
        {
            using (Bitmap bmp = new Bitmap(128, 128))
            using (Graphics g = Graphics.FromImage(bmp))
            {
                g.Clear(Color.Black);
                g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAlias;

                if (apps == null || apps.Count == 0)
                {
                    using (Font f = new Font("Arial", 14, FontStyle.Bold))
                    {
                        g.DrawString("SETUP", f, Brushes.White, 10, 40);
                        g.DrawString("REQUIRED", f, Brushes.Yellow, 10, 60);
                    }
                }
                else
                {
                    Font font = new Font("Arial", 18, FontStyle.Bold);
                    int rowHeight = 128 / 4;

                    for (int i = 0; i < apps.Count && i < 4; i++)
                    {
                        var app = apps[i];

                        // Chiamiamo il nuovo metodo che restituisce VolumeData
                        VolumeData data = AudioHelper.GetVolumeForProcess(app.ExeName ?? "");

                        // Colore base scelto dall'utente
                        Color userColor = Color.White;
                        try { userColor = ColorTranslator.FromHtml(app.HexColor); } catch { }

                        string textToDraw;
                        Brush brushToUse;

                        // LOGICA MUTO
                        if (data.IsMuted)
                        {
                            // Se è MUTO: Scriviamo "X" in ROSSO
                            textToDraw = $"{app.Label}: X";
                            brushToUse = Brushes.Red;
                        }
                        else
                        {
                            // Se è NORMALE: Scriviamo il numero col colore utente
                            textToDraw = $"{app.Label}: {data.Volume:0}";
                            brushToUse = new SolidBrush(userColor);
                        }

                        // Disegna
                        g.DrawString(textToDraw, font, brushToUse, 5, (i * rowHeight) + 8);

                        // Se abbiamo creato un pennello personalizzato (SolidBrush), liberiamolo
                        if (!data.IsMuted)
                        {
                            brushToUse.Dispose();
                        }
                    }
                }

                using (MemoryStream ms = new MemoryStream())
                {
                    bmp.Save(ms, System.Drawing.Imaging.ImageFormat.Png);
                    return "data:image/png;base64," + Convert.ToBase64String(ms.ToArray());
                }
            }
        }
    }
}