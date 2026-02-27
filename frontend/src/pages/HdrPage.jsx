import React, { useEffect, useRef, useState, useCallback } from "react";
import {
  processHDR,
  listMissions,
  getMissionGroups,
  getMissionHdrs,
  adjustHDR,
  enhanceWithTopaz,
  API_BASE,
} from "../api";

const defaultSettings = {
  // Light
  exposure: 1.0,
  contrast: 1.0,
  highlights: 0.0,
  shadows: 0.0,
  whites: 0.0,
  blacks: 0.0,
  // Color
  temperature: 0,
  tint: 0,
  vibrance: 0.0,
  saturation: 1.0,
  // Detail
  sharpening: 0,
  noise_reduction: 0,
  // Effects
  dehaze: 0.0,
  vignette: 0.0,
  grain: 0,
  // Processing
  merge_method: "mertens",
  output_format: "jpg",
};

export default function HdrPage({ fullscreen = false, toggleFullscreen }) {
  const [missionId, setMissionId] = useState("");
  const [outputName, setOutputName] = useState("");
  const [bracketPathsText, setBracketPathsText] = useState("");
  const [settings, setSettings] = useState(defaultSettings);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [error, setError] = useState("");
  const [result, setResult] = useState(null);
  const [missions, setMissions] = useState([]);
  const [groups, setGroups] = useState({});
  const [loadingGroups, setLoadingGroups] = useState(false);
  const [hdrFiles, setHdrFiles] = useState([]);
  const [preview, setPreview] = useState("");
  const [autoPreview, setAutoPreview] = useState(true);
  const [selectedHdr, setSelectedHdr] = useState(null);
  const [localPreview, setLocalPreview] = useState("");
  const [imgDims, setImgDims] = useState({ w: 0, h: 0 });
  const [glReady, setGlReady] = useState(false);
  const [renderMode, setRenderMode] = useState("LOAD");
  const [renderNote, setRenderNote] = useState("");
  const [leftPinned, setLeftPinned] = useState(true);
  const [rotation, setRotation] = useState(0);
  const canvasRef = useRef(null);
  const glCanvasRef = useRef(null);
  const rafRef = useRef(null);
  const objectUrlRef = useRef(null);
  const sourceImageDataRef = useRef(null);
  const sourceImageRef = useRef(null);
  const glRef = useRef(null);
  const glStateRef = useRef(null);

  // Undo/Redo history
  const [history, setHistory] = useState([defaultSettings]);
  const [historyIndex, setHistoryIndex] = useState(0);

  // Histogram data
  const [histogramData, setHistogramData] = useState(null);
  const histogramRef = useRef(null);

  // B19: use shared API_BASE from api.js
  const apiBase = API_BASE;

  // B4: Refs to avoid stale closures in keyboard handler
  const historyRef = useRef(history);
  const historyIndexRef = useRef(historyIndex);
  const busyRef = useRef(busy);
  const missionIdRef = useRef(missionId);
  const bracketPathsTextRef = useRef(bracketPathsText);
  const settingsRef = useRef(settings);
  const fullscreenRef = useRef(fullscreen);

  // Keep refs in sync
  historyRef.current = history;
  historyIndexRef.current = historyIndex;
  busyRef.current = busy;
  missionIdRef.current = missionId;
  bracketPathsTextRef.current = bracketPathsText;
  settingsRef.current = settings;
  fullscreenRef.current = fullscreen;

  // Keyboard shortcuts for undo/redo
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.ctrlKey || e.metaKey) {
        if (e.key === 'z' && !e.shiftKey) {
          e.preventDefault();
          // Undo
          if (historyIndexRef.current > 0) {
            const newIndex = historyIndexRef.current - 1;
            setHistoryIndex(newIndex);
            setSettings(historyRef.current[newIndex]);
          }
        } else if ((e.key === 'y') || (e.key === 'z' && e.shiftKey)) {
          e.preventDefault();
          // Redo
          if (historyIndexRef.current < historyRef.current.length - 1) {
            const newIndex = historyIndexRef.current + 1;
            setHistoryIndex(newIndex);
            setSettings(historyRef.current[newIndex]);
          }
        } else if (e.key === 'r') {
          e.preventDefault();
          // Reset all settings
          setSettings(defaultSettings);
          setHistory([defaultSettings]);
          setHistoryIndex(0);
        }
      } else if (e.target.tagName !== 'INPUT' && e.target.tagName !== 'TEXTAREA' && e.target.tagName !== 'SELECT') {
        // Non-modifier shortcuts (only outside text inputs)
        switch (e.key) {
          case 'f':
          case 'F':
            if (toggleFullscreen) toggleFullscreen();
            break;
          case 'Escape':
            if (fullscreenRef.current && toggleFullscreen) toggleFullscreen();
            break;
          case ' ':
            e.preventDefault();
            if (!busyRef.current && missionIdRef.current && bracketPathsTextRef.current.trim()) {
              handleProcess(settingsRef.current, missionIdRef.current, bracketPathsTextRef.current);
            }
            break;
          default:
            break;
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [toggleFullscreen]); // only toggleFullscreen is needed as prop dependency


  useEffect(() => {
    async function load() {
      try {
        const data = await listMissions();
        setMissions(data);
      } catch (err) {
        setError(err.message);
      }
    }
    load();
  }, []);

  useEffect(() => {
    return () => {
      if (objectUrlRef.current) {
        URL.revokeObjectURL(objectUrlRef.current);
        objectUrlRef.current = null;
      }
      if (glStateRef.current) {
        const { texture } = glStateRef.current;
        const gl = glRef.current;
        if (gl && texture) gl.deleteTexture(texture);
      }
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, []);

  useEffect(() => {
    if (fullscreen) setLeftPinned(false);
  }, [fullscreen]);

  const triggerPreview = useCallback((
    nextSettings = settings,
    nextMissionId = missionId,
    nextBrackets = bracketPathsText,
  ) => {
    if (!autoPreview) return;
    if (selectedHdr) {
      if (!glStateRef.current && !sourceImageDataRef.current) return;
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
      rafRef.current = requestAnimationFrame(() => renderLocalPreview(nextSettings));
    } else {
      if (!nextMissionId || !nextBrackets.trim()) return;
      handleProcess(nextSettings, nextMissionId, nextBrackets);
    }
  }, [settings, missionId, bracketPathsText, autoPreview, selectedHdr]);



  async function handleProcess(settingsToUse, missionVal, bracketText) {
    setError("");
    setResult(null);
    const paths = bracketText
      .split(/\r?\n/)
      .map((p) => p.trim())
      .filter(Boolean);
    if (!missionVal || paths.length === 0) {
      setError("Renseigne mission_id et au moins un chemin de bracket.");
      return;
    }
    setBusy(true);
    try {
      const data = await processHDR({
        missionId: Number(missionVal),
        bracketPaths: paths,
        settings: settingsToUse,
        outputName,
      });
      setResult(data);
      setMessage(`HDR generated: ${data.output_path}`);
      const filename = data.output_path.split(/[/\\]/).pop();
      if (filename) {
        setPreview(withBust(`${apiBase}/missions/${missionVal}/hdrs/files/${filename}`));
      }
      const hdrs = await getMissionHdrs(missionVal);
      setHdrFiles(hdrs);
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  async function handleAdjust(settingsToUse = settings, missionVal = missionId, hdr = selectedHdr) {
    if (!missionVal || !hdr) return;
    setError("");
    setResult(null);
    setBusy(true);
    try {
      const data = await adjustHDR({
        missionId: Number(missionVal),
        inputFilename: hdr.name,
        outputName,
        settings: settingsToUse,
      });
      setResult(data);
      setMessage(`HDR ajuste: ${data.output_path}`);
      const filename = data.output_path.split(/[/\\]/).pop();
      if (filename) {
        setPreview(withBust(`${apiBase}/missions/${missionVal}/hdrs/files/${filename}`));
      }
      const hdrs = await getMissionHdrs(missionVal);
      setHdrFiles(hdrs);
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  function markFallback(note) {
    setGlReady(false);
    setRenderMode("CPU");
    if (note) setRenderNote(note);
  }

  function renderLocalPreview(currentSettings = settings) {
    if (renderWithWebGL(currentSettings)) return;
    if (glReady) setGlReady(false);
    if (renderMode !== "CPU") {
      setRenderMode("CPU");
      if (!renderNote) setRenderNote("CPU fallback");
    }
    const src = sourceImageDataRef.current;
    const canvas = canvasRef.current;
    if (!src || !canvas) return;
    const ctx = canvas.getContext("2d", { willReadFrequently: true });
    const clone = new ImageData(new Uint8ClampedArray(src.data), src.width, src.height);
    applySettingsToImageData(clone, currentSettings);
    canvas.width = src.width;
    canvas.height = src.height;
    ctx.putImageData(clone, 0, 0);
    setLocalPreview(canvas.toDataURL("image/jpeg", 0.92));
  }

  function applySettingsToImageData(imageData, s) {
    const d = imageData.data;
    const exp = s.exposure ?? 1;
    const ctr = s.contrast ?? 1;
    const sat = s.saturation ?? 1;
    const hlt = s.highlights ?? 0;
    const shd = s.shadows ?? 0;
    for (let i = 0; i < d.length; i += 4) {
      let r = d[i] / 255;
      let g = d[i + 1] / 255;
      let b = d[i + 2] / 255;
      r = 0.5 + (r * exp - 0.5) * ctr;
      g = 0.5 + (g * exp - 0.5) * ctr;
      b = 0.5 + (b * exp - 0.5) * ctr;
      if (hlt !== 0) {
        r = r > 0.6 ? r - hlt * (r - 0.6) : r;
        g = g > 0.6 ? g - hlt * (g - 0.6) : g;
        b = b > 0.6 ? b - hlt * (b - 0.6) : b;
      }
      if (shd !== 0) {
        r = r < 0.4 ? r + shd * (0.4 - r) : r;
        g = g < 0.4 ? g + shd * (0.4 - g) : g;
        b = b < 0.4 ? b + shd * (0.4 - b) : b;
      }
      if (sat !== 1) {
        const gray = 0.299 * r + 0.587 * g + 0.114 * b;
        r = gray + (r - gray) * sat;
        g = gray + (g - gray) * sat;
        b = gray + (b - gray) * sat;
      }
      r = Math.min(1, Math.max(0, r));
      g = Math.min(1, Math.max(0, g));
      b = Math.min(1, Math.max(0, b));
      d[i] = Math.round(r * 255);
      d[i + 1] = Math.round(g * 255);
      d[i + 2] = Math.round(b * 255);
    }
  }

  const loadSelectedHdrImage = useCallback((hdr) => {
    setError("");
    setGlReady(false);
    setLocalPreview("");
    setImgDims({ w: 0, h: 0 });
    setRenderMode("LOAD");
    setRenderNote("Loading image");
    setRotation(0);
    if (objectUrlRef.current) {
      URL.revokeObjectURL(objectUrlRef.current);
      objectUrlRef.current = null;
    }
    fetch(withBust(`${apiBase}${hdr.url}`))
      .then((res) => {
        if (!res.ok) throw new Error("Erreur de chargement de l'image");
        return res.blob();
      })
      .then((blob) => {
        if (typeof createImageBitmap === "function") {
          return createImageBitmap(blob, { imageOrientation: "from-image", premultiplyAlpha: "none" });
        }
        return new Promise((resolve, reject) => {
          const url = URL.createObjectURL(blob);
          objectUrlRef.current = url;
          const img = new Image();
          img.onload = () => {
            URL.revokeObjectURL(url);
            objectUrlRef.current = null;
            resolve(img);
          };
          img.onerror = () => reject(new Error("Image load failed"));
          img.src = url;
        });
      })
      .then((bitmap) => {
        const MAX_DIM = 4096;
        let w = bitmap.width || bitmap.naturalWidth || 0;
        let h = bitmap.height || bitmap.naturalHeight || 0;

        // Downscale for performance
        if (w > MAX_DIM || h > MAX_DIM) {
          const scale = Math.min(MAX_DIM / w, MAX_DIM / h);
          w = Math.round(w * scale);
          h = Math.round(h * scale);
        }

        setImgDims({ w, h });
        // sourceImageRef.current = bitmap; // Not used for drawing anymore

        // 1. Create a temp canvas (not in DOM) to perform the resize
        const tempCanvas = document.createElement("canvas");
        tempCanvas.width = w;
        tempCanvas.height = h;
        const ctx = tempCanvas.getContext("2d", { willReadFrequently: true });

        // 2. Draw scaled image
        ctx.drawImage(bitmap, 0, 0, w, h);

        // 3. Update CPU fallback data
        sourceImageDataRef.current = ctx.getImageData(0, 0, w, h);

        // 4. Pass the SCALED canvas to WebGL init
        initWebGLFromBitmap(tempCanvas, w, h);
        renderLocalPreview(settings);
      })
      .catch((err) => setError(err.message));
  }, [apiBase, settings]);

  const handleSubmit = useCallback(async (e) => {
    if (e && e.preventDefault) e.preventDefault();
    if (selectedHdr) {
      await handleAdjust();
    } else {
      await handleProcess(settings, missionId, bracketPathsText);
    }
  }, [selectedHdr, settings, missionId, bracketPathsText]);

  const loadGroups = useCallback(async (mid) => {
    if (!mid) return;
    setLoadingGroups(true);
    setError("");
    try {
      const data = await getMissionGroups(mid);
      setGroups(data);
      const hdrs = await getMissionHdrs(mid);
      setHdrFiles(hdrs);
      if (hdrs.length) {
        setPreview(withBust(`${apiBase}${hdrs[hdrs.length - 1].url}`));
        setResult({ output_path: hdrs[hdrs.length - 1].path });
      } else {
        setPreview("");
      }
    } catch (err) {
      setError(err.message);
    } finally {
      setLoadingGroups(false);
    }
  }, [apiBase]);

  const useGroup = useCallback((name, paths) => {
    setSelectedHdr(null);
    setLocalPreview("");
    setGlReady(false);
    setBracketPathsText(paths.join("\n"));
    setOutputName(`${name}_hdr`);
    triggerPreview(settings, missionId, paths.join("\n"));
  }, [settings, missionId, triggerPreview]);

  function createShader(gl, type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      const stage = type === gl.VERTEX_SHADER ? "vertex" : "fragment";
      const info = gl.getShaderInfoLog(shader);
      console.error(info);
      markFallback(`WebGL ${stage} shader failed`);
      gl.deleteShader(shader);
      return null;
    }
    return shader;
  }

  function createProgram(gl, vsSource, fsSource) {
    const vs = createShader(gl, gl.VERTEX_SHADER, vsSource);
    const fs = createShader(gl, gl.FRAGMENT_SHADER, fsSource);
    if (!vs || !fs) return null;
    const program = gl.createProgram();
    gl.attachShader(program, vs);
    gl.attachShader(program, fs);
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      const info = gl.getProgramInfoLog(program);
      console.error(info);
      markFallback("WebGL program link failed");
      gl.deleteProgram(program);
      return null;
    }
    return program;
  }

  function initGL(gl) {
    const vsSource = `
      attribute vec2 a_position;
      attribute vec2 a_texCoord;
      varying vec2 v_texCoord;
      void main() {
        gl_Position = vec4(a_position, 0.0, 1.0);
        v_texCoord = a_texCoord;
      }
    `;
    const fsSource = `
      precision highp float;
      varying vec2 v_texCoord;
      uniform sampler2D u_image;
      uniform float u_exposure;
      uniform float u_contrast;
      uniform float u_highlights;
      uniform float u_shadows;
      uniform float u_saturation;
      uniform float u_temperature;
      uniform float u_tint;
      uniform float u_vibrance;
      uniform float u_whites;
      uniform float u_blacks;
      uniform float u_dehaze;
      uniform float u_vignette;

      void main() {
        vec4 c = texture2D(u_image, v_texCoord);
        vec3 color = c.rgb;

        // 1. Exposure
        color *= u_exposure;

        // 2. Temperature (warm ↔ cool)
        if (u_temperature != 0.0) {
          float t = u_temperature * 0.004;
          color.r += t;
          color.b -= t;
        }

        // 3. Tint (green ↔ magenta)
        if (u_tint != 0.0) {
          float ti = u_tint * 0.004;
          color.g += ti;
          color.r -= ti * 0.5;
          color.b -= ti * 0.5;
        }

        // 4. Contrast
        color = 0.5 + (color - 0.5) * u_contrast;

        // 5. Highlights recovery
        if (u_highlights != 0.0) {
          vec3 luma = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
          float hl = smoothstep(0.5, 1.0, luma.r);
          color = mix(color, color * (1.0 - u_highlights * 0.5), hl);
        }

        // 6. Shadows lift
        if (u_shadows != 0.0) {
          vec3 luma = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
          float sh = 1.0 - smoothstep(0.0, 0.5, luma.r);
          color = mix(color, color + u_shadows * 0.3, sh);
        }

        // 7. Whites
        if (u_whites != 0.0) {
          vec3 luma = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
          float wh = smoothstep(0.7, 1.0, luma.r);
          color += u_whites * 0.2 * wh;
        }

        // 8. Blacks
        if (u_blacks != 0.0) {
          vec3 luma = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
          float bl = 1.0 - smoothstep(0.0, 0.3, luma.r);
          color += u_blacks * 0.2 * bl;
        }

        // 9. Dehaze (clarity — midtone contrast)
        if (u_dehaze != 0.0) {
          float gray = dot(color, vec3(0.299, 0.587, 0.114));
          float mid = smoothstep(0.1, 0.5, gray) * (1.0 - smoothstep(0.5, 0.9, gray));
          color = mix(color, 0.5 + (color - 0.5) * (1.0 + u_dehaze), mid);
        }

        // 10. Vibrance (selective saturation — boost weak colors more)
        float gray2 = dot(color, vec3(0.299, 0.587, 0.114));
        if (u_vibrance != 0.0) {
          float maxC = max(color.r, max(color.g, color.b));
          float minC = min(color.r, min(color.g, color.b));
          float sat = (maxC > 0.001) ? (maxC - minC) / maxC : 0.0;
          float vibAdj = u_vibrance * (1.0 - sat) * 0.5;
          color = gray2 + (color - gray2) * (1.0 + vibAdj);
        }

        // 11. Saturation
        float gray3 = dot(color, vec3(0.299, 0.587, 0.114));
        color = gray3 + (color - gray3) * u_saturation;

        // 12. Vignette
        if (u_vignette != 0.0) {
          vec2 uv = v_texCoord * 2.0 - 1.0;
          float dist = length(uv) * 0.707;
          float vig = 1.0 - u_vignette * dist * dist;
          color *= clamp(vig, 0.0, 1.0);
        }

        gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
      }
    `;
    const program = createProgram(gl, vsSource, fsSource);
    if (!program) return null;
    const posLoc = gl.getAttribLocation(program, "a_position");
    const texLoc = gl.getAttribLocation(program, "a_texCoord");
    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    const quad = new Float32Array([
      -1, -1, 0, 0,
      1, -1, 1, 0,
      -1, 1, 0, 1,
      -1, 1, 0, 1,
      1, -1, 1, 0,
      1, 1, 1, 1,
    ]);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    return { program, posLoc, texLoc, buffer };
  }

  function initWebGLFromBitmap(bitmap, w, h) {
    const canvas = glCanvasRef.current;
    if (!canvas) {
      markFallback("WebGL canvas missing");
      return;
    }
    const gl = canvas.getContext("webgl", { premultipliedAlpha: false }) || canvas.getContext("webgl2", { premultipliedAlpha: false });
    if (!gl) {
      markFallback("WebGL context unavailable");
      return;
    }
    glRef.current = gl;
    canvas.width = w;
    canvas.height = h;

    const state = initGL(gl);
    if (!state) {
      markFallback("WebGL init failed");
      return;
    }
    // B25: Delete old texture to prevent GPU memory leak when re-selecting images
    if (glStateRef.current?.texture) {
      gl.deleteTexture(glStateRef.current.texture);
    }
    glStateRef.current = state;

    const texture = gl.createTexture();
    if (!texture) {
      markFallback("WebGL texture failed");
      return;
    }
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, bitmap);

    glStateRef.current.texture = texture;
    setLocalPreview("");
    setGlReady(true);
    setRenderMode("WEBGL");
    setRenderNote("");
  }

  function renderWithWebGL(s) {
    const gl = glRef.current;
    const state = glStateRef.current;
    const canvas = glCanvasRef.current;
    if (!gl || !state || !canvas) return false;
    try {
      const { program, posLoc, texLoc, buffer, texture } = state;
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
      gl.useProgram(program);
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.enableVertexAttribArray(posLoc);
      gl.enableVertexAttribArray(texLoc);
      gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 16, 0);
      gl.vertexAttribPointer(texLoc, 2, gl.FLOAT, false, 16, 8);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.uniform1i(gl.getUniformLocation(program, "u_image"), 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_exposure"), s.exposure ?? 1);
      gl.uniform1f(gl.getUniformLocation(program, "u_contrast"), s.contrast ?? 1);
      gl.uniform1f(gl.getUniformLocation(program, "u_highlights"), s.highlights ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_shadows"), s.shadows ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_saturation"), s.saturation ?? 1);
      gl.uniform1f(gl.getUniformLocation(program, "u_temperature"), s.temperature ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_tint"), s.tint ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_vibrance"), s.vibrance ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_whites"), s.whites ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_blacks"), s.blacks ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_dehaze"), s.dehaze ?? 0);
      gl.uniform1f(gl.getUniformLocation(program, "u_vignette"), s.vignette ?? 0);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      return true;
    } catch (err) {
      console.warn("WebGL render failed, fallback CPU", err);
      markFallback("WebGL render failed");
      return false;
    }
  }

  const handleTopaz = useCallback(async (hdr, model = "denoise", params = {}) => {
    if (!hdr) return;
    setError("");
    setMessage(`Topaz AI (${model}): envoi en cours...`);
    setBusy(true);
    try {
      const data = await enhanceWithTopaz({
        path: hdr.path,
        model: params.model || model,
        outputName: hdr.name.replace(/\.[^.]+$/, "") + `_${model}.jpg`,
        strength: params.strength,
        scale_factor: params.scale_factor,
        face_enhancement: params.face_enhancement,
        fix_compression: params.fix_compression,
      });
      setMessage(`Topaz terminé: ${data.output_path.split(/[/\\]/).pop()}`);
      // Refresh list
      const hdrs = await getMissionHdrs(missionId);
      setHdrFiles(hdrs);
      // Show result
      if (data.output_path) {
        const filename = data.output_path.split(/[/\\]/).pop();
        setPreview(withBust(`${apiBase}/missions/${missionId}/hdrs/files/${filename}`));
        setResult({ output_path: data.output_path });
      }
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }, [missionId, apiBase]);

  const update = useCallback((field, value) => {
    setSettings((s) => {
      const next = { ...s, [field]: value };
      triggerPreview(next);

      // Add to history for undo/redo (debounced by field)
      setHistory((prev) => {
        // Truncate future history if we're not at the end
        const newHistory = prev.slice(0, historyIndex + 1);
        // Only add if different from current
        const current = newHistory[newHistory.length - 1];
        if (JSON.stringify(current) !== JSON.stringify(next)) {
          newHistory.push(next);
          // Limit history to 50 entries
          if (newHistory.length > 50) newHistory.shift();
          setHistoryIndex(newHistory.length - 1);
        }
        return newHistory;
      });

      return next;
    });
  }, [triggerPreview, historyIndex]);

  return (
    <div className="hdr-page">
      <div className={`hdr-layout ${!leftPinned ? "left-autohide" : ""} ${fullscreen ? "fullscreen" : ""}`}>

        {/* Floating toggle for when drawer is closed */}
        <div
          className="drawer-toggle-float"
          onClick={() => setLeftPinned(true)}
          title="Ouvrir le menu"
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M4 12h16M4 6h16M4 18h16" /></svg>
        </div>

        {/* New Drawer Panel */}
        <div className="hdr-drawer">
          <div className="hdr-drawer-header">
            <h3>HDR Studio</h3>
            <button
              className="icon-btn"
              onClick={() => setLeftPinned(false)}
              title="Fermer le menu"
            >
              <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M15 19l-7-7 7-7" /></svg>
            </button>
          </div>

          <LeftPanelBody
            selectedHdr={selectedHdr}
            error={error}
            message={message}
            handleSubmit={handleSubmit}
            missionId={missionId}
            setMissionId={setMissionId}
            loadGroups={loadGroups}
            settings={settings}
            bracketPathsText={bracketPathsText}
            triggerPreview={triggerPreview}
            missions={missions}
            outputName={outputName}
            setOutputName={setOutputName}
            setBracketPathsText={setBracketPathsText}
            busy={busy}
            groups={groups}
            loadingGroups={loadingGroups}
            useGroup={useGroup}
            renderGroupThumb={renderGroupThumb}
            hdrFiles={hdrFiles}
            apiBase={apiBase}
            setSelectedHdr={setSelectedHdr}
            setPreview={setPreview}
            setLocalPreview={setLocalPreview}
            loadSelectedHdrImage={loadSelectedHdrImage}
            setMessage={setMessage}
          />
        </div>

        <PreviewSection
          selectedHdr={selectedHdr}
          renderMode={renderMode}
          renderNote={renderNote}
          rotation={rotation}
          setRotation={setRotation}
          result={result}
          setResult={setResult}
          glCanvasRef={glCanvasRef}
          glReady={glReady}
          localPreview={localPreview}
          preview={preview}
        />

        <RightPanel
          fullscreen={fullscreen}
          toggleFullscreen={toggleFullscreen}
          settings={settings}
          update={update}
          selectedHdr={selectedHdr}
          handleTopaz={handleTopaz}
          busy={busy}
          autoPreview={autoPreview}
          handleSubmit={handleSubmit}
          missionId={missionId}
          bracketPathsText={bracketPathsText}
          glCanvasRef={glCanvasRef}
          historyIndex={historyIndex}
          historyLength={history.length}
        />

        <canvas ref={canvasRef} style={{ display: "none" }} aria-hidden="true"></canvas>
      </div >
    </div >
  );
}

function round3(value) {
  return Math.round(value * 1000) / 1000;
}

function Slider({ label, min, max, step, value, onChange, defaultValue, panel, bipolar, dataType }) {
  const cls = `slider-control${bipolar ? ' bipolar' : ''}`;
  return (
    <div className={cls} data-panel={panel} data-type={dataType}>
      <div className="slider-header">
        <span>{label}</span>
        <input
          className="slider-input"
          type="number"
          value={value}
          step={step}
          min={min}
          max={max}
          onChange={(e) => {
            const next = parseFloat(e.target.value);
            if (!Number.isNaN(next)) onChange(next);
          }}
          onDoubleClick={() => onChange(defaultValue)}
        />
      </div>
      <div className="slider-track-wrap">
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          onChange={(e) => onChange(parseFloat(e.target.value))}
          onDoubleClick={() => onChange(defaultValue)}
        />
      </div>
    </div>
  );
}

function renderGroupThumb(groupName, hdrFiles, apiBase) {
  const match = hdrFiles.find((h) =>
    h.name.toLowerCase().includes(groupName.toLowerCase()),
  );
  if (!match) {
    return (
      <div className="hdr-thumb placeholder" />
    );
  }
  return (
    <img
      src={`${apiBase}${match.url}`}
      alt={groupName}
      className="hdr-thumb"
      loading="lazy"
      decoding="async"
    />
  );
}

function withBust(url) {
  const sep = url.includes("?") ? "&" : "?";
  return `${url}${sep}t=${Date.now()}`;
}

const RightPanel = React.memo(({
  fullscreen, toggleFullscreen, settings, update, selectedHdr, handleTopaz, busy,
  autoPreview, handleSubmit, missionId, bracketPathsText, glCanvasRef, historyIndex, historyLength
}) => {
  const [openPanels, setOpenPanels] = useState({ light: true, color: true, detail: false, effects: false });

  const togglePanel = (panel) => {
    setOpenPanels(prev => ({ ...prev, [panel]: !prev[panel] }));
  };

  return (
    <div className="card hdr-panel hdr-panel-right">
      {/* Histogram */}
      {selectedHdr && <Histogram glCanvasRef={glCanvasRef} />}

      {/* History indicator */}
      {historyLength > 1 && (
        <div className="history-indicator" title="Ctrl+Z: Annuler, Ctrl+Y: Rétablir, Ctrl+R: Réinitialiser">
          <span className="history-badge">{historyIndex + 1}/{historyLength}</span>
          <span className="history-hint"><svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><rect x="2" y="4" width="20" height="16" rx="2" /><path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01M8 12h8M6 16h12" /></svg> Ctrl+Z/Y</span>
        </div>
      )}

      {fullscreen && (
        <div style={{ marginBottom: 12 }}>
          <button
            className="btn secondary full-width"
            onClick={() => toggleFullscreen && toggleFullscreen()}
            style={{ background: "rgba(255,255,255,0.1)" }}
          >
            Sortir Plein Ecran
          </button>
        </div>
      )}

      {/* LIGHT PANEL */}
      <AccordionPanel title="Lumière" icon={<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="12" cy="12" r="5" /><path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42" /></svg>} isOpen={openPanels.light} onToggle={() => togglePanel('light')}>
        <Slider label="Exposition" min={-2} max={2} step={0.05}
          value={round3(settings.exposure - 1)} defaultValue={0}
          onChange={(v) => update("exposure", v + 1)} panel="light" bipolar />
        <Slider label="Contraste" min={-0.5} max={0.5} step={0.05}
          value={round3(settings.contrast - 1)} defaultValue={0}
          onChange={(v) => update("contrast", v + 1)} panel="light" bipolar />
        <Slider label="Hautes lumières" min={-1} max={1} step={0.05}
          value={round3(-settings.highlights)} defaultValue={0}
          onChange={(v) => update("highlights", -v)} panel="light" bipolar />
        <Slider label="Ombres" min={-1} max={1} step={0.05}
          value={round3(settings.shadows)} defaultValue={0}
          onChange={(v) => update("shadows", v)} panel="light" bipolar />
        <Slider label="Blancs" min={-1} max={1} step={0.05}
          value={round3(settings.whites || 0)} defaultValue={0}
          onChange={(v) => update("whites", v)} panel="light" bipolar />
        <Slider label="Noirs" min={-1} max={1} step={0.05}
          value={round3(settings.blacks || 0)} defaultValue={0}
          onChange={(v) => update("blacks", v)} panel="light" bipolar />
      </AccordionPanel>

      {/* COLOR PANEL */}
      <AccordionPanel title="Couleur" icon={<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="13.5" cy="6.5" r="2.5" /><circle cx="17.5" cy="10.5" r="2.5" /><circle cx="8.5" cy="7.5" r="2.5" /><circle cx="6.5" cy="12" r="2.5" /><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10c.9 0 1.5-.7 1.5-1.5 0-.4-.1-.7-.4-1-.3-.3-.4-.6-.4-1 0-.8.7-1.5 1.5-1.5H16c3.3 0 6-2.7 6-6 0-5.5-4.5-9-10-9z" /></svg>} isOpen={openPanels.color} onToggle={() => togglePanel('color')}>
        <Slider label="Température" min={-100} max={100} step={1}
          value={settings.temperature || 0} defaultValue={0}
          onChange={(v) => update("temperature", v)} panel="color" bipolar dataType="temperature" />
        <Slider label="Teinte" min={-100} max={100} step={1}
          value={settings.tint || 0} defaultValue={0}
          onChange={(v) => update("tint", v)} panel="color" bipolar dataType="tint" />
        <Slider label="Vibrance" min={-1} max={1} step={0.05}
          value={round3(settings.vibrance || 0)} defaultValue={0}
          onChange={(v) => update("vibrance", v)} panel="color" bipolar />
        <Slider label="Saturation" min={-1} max={1} step={0.05}
          value={round3(settings.saturation - 1)} defaultValue={0}
          onChange={(v) => update("saturation", v + 1)} panel="color" bipolar />
      </AccordionPanel>

      {/* DETAIL PANEL */}
      <AccordionPanel title="Détail" icon={<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M6 8l4 4-4 4M12 16h6" /></svg>} isOpen={openPanels.detail} onToggle={() => togglePanel('detail')}>
        <Slider label="Netteté" min={0} max={100} step={1}
          value={settings.sharpening || 0} defaultValue={0}
          onChange={(v) => update("sharpening", v)} panel="detail" />
        <Slider label="Réduction bruit" min={0} max={100} step={1}
          value={settings.noise_reduction || 0} defaultValue={0}
          onChange={(v) => update("noise_reduction", v)} panel="detail" />
      </AccordionPanel>

      {/* EFFECTS PANEL */}
      <AccordionPanel title="Effets" icon={<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M12 3l1.9 5.8h6.1l-5 3.6 1.9 5.8-5-3.6-5 3.6 1.9-5.8-5-3.6h6.1z" /></svg>} isOpen={openPanels.effects} onToggle={() => togglePanel('effects')}>
        <Slider label="Clarté (Dehaze)" min={-1} max={1} step={0.05}
          value={round3(settings.dehaze || 0)} defaultValue={0}
          onChange={(v) => update("dehaze", v)} panel="effects" bipolar />
        <Slider label="Vignette" min={-1} max={1} step={0.05}
          value={round3(settings.vignette || 0)} defaultValue={0}
          onChange={(v) => update("vignette", v)} panel="effects" bipolar />
      </AccordionPanel>

      {/* EXPORT */}
      <div className="settings-group" style={{ marginTop: 12 }}>
        <div className="settings-header">Export</div>
        <label className="select-row">
          <span className="label-text">Format</span>
          <select
            className="field small"
            value={settings.output_format}
            onChange={(e) => update("output_format", e.target.value)}
            style={{ width: "100%", marginTop: 4 }}
          >
            <option value="jpg">JPG (Web)</option>
            <option value="png">PNG (Lossless)</option>
            <option value="tiff">TIFF (Print)</option>
          </select>
        </label>
      </div>

      {/* TOPAZ AI */}
      {selectedHdr && (
        <TopazPanel handleTopaz={handleTopaz} selectedHdr={selectedHdr} busy={busy} />
      )}

      {/* SAVE BUTTON */}
      {(selectedHdr || !autoPreview) && (
        <button
          className="btn full-width topaz-save-btn"
          type="button"
          onClick={() => handleSubmit()}
          disabled={busy || (!selectedHdr && (!missionId || !bracketPathsText.trim()))}
        >
          {busy ? "Traitement..." : selectedHdr ? "Sauvegarder" : "Appliquer"}
        </button>
      )}
    </div>
  );
});

// Accordion Panel Component
const AccordionPanel = React.memo(({ title, icon, isOpen, onToggle, children }) => {
  return (
    <div className={`accordion-panel ${isOpen ? 'open' : ''}`}>
      <button className="accordion-header" onClick={onToggle} type="button">
        <span className="accordion-icon">{icon}</span>
        <span className="accordion-title">{title}</span>
        <svg className={`accordion-chevron ${isOpen ? 'open' : ''}`} width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <path d="M6 9l6 6 6-6" />
        </svg>
      </button>
      {isOpen && (
        <div className="accordion-content">
          {children}
        </div>
      )}
    </div>
  );
});

// Topaz AI Panel Component
const TOPAZ_MODELS = {
  enhance: [
    { value: "Standard V2", label: "Standard V2" },
    { value: "Low Resolution V2", label: "Low Resolution" },
    { value: "High Fidelity V2", label: "High Fidelity" },
    { value: "CGI", label: "CGI" },
  ],
  denoise: [
    { value: "auto", label: "Autopilot" },
  ],
  sharpen: [
    { value: "auto", label: "Auto Sharpen" },
    { value: "wildlife", label: "Wildlife" },
    { value: "portrait", label: "Portrait" },
  ],
};

const TopazIcon = ({ d, size = 16 }) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
    <path d={d} />
  </svg>
);

const TOPAZ_OPS = [
  { key: "denoise", label: "Denoise", icon: <TopazIcon d="M11 5L6 9H2v6h4l5 4V5zM23 9l-6 6M17 9l6 6" />, desc: "Suppression du bruit" },
  { key: "sharpen", label: "Sharpen", icon: <TopazIcon d="M12 2L2 12l10 10 10-10L12 2zM12 8v8M8 12h8" />, desc: "Netteté IA" },
  { key: "enhance", label: "Upscale", icon: <TopazIcon d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7" />, desc: "Agrandissement IA" },
  { key: "enhance", label: "Face AI", icon: <TopazIcon d="M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2M12 3a4 4 0 100 8 4 4 0 000-8z" />, desc: "Restauration visages", isFace: true },
  { key: "enhance", label: "Fix JPEG", icon: <TopazIcon d="M4.5 16.5c-1.5 1.26-2 5-2 5s3.74-.5 5-2c.71-.84.7-2.13-.09-2.91a2.18 2.18 0 00-2.91-.09zM12 15l-3-3M22 2l-5 5M5.5 21.5L2 22l.5-3.5L17 4l3 3L5.5 21.5z" />, desc: "Artefacts compression", isFixComp: true },
];

const TopazPanel = React.memo(({ handleTopaz, selectedHdr, busy }) => {
  const [showOptions, setShowOptions] = useState(false);
  const [topazModel, setTopazModel] = useState("Standard V2");
  const [topazStrength, setTopazStrength] = useState(0.8);
  const [topazScale, setTopazScale] = useState("2x");
  const [faceEnhance, setFaceEnhance] = useState(false);
  const [activeOp, setActiveOp] = useState(null);

  const handleOp = (op) => {
    setActiveOp(op.label);
    const params = {
      model: op.key === "denoise" ? "auto" : op.key === "sharpen" ? topazModel : topazModel,
      strength: topazStrength,
    };
    if (op.key === "enhance") {
      params.scale_factor = topazScale;
      params.face_enhancement = op.isFace || faceEnhance;
      params.fix_compression = !!op.isFixComp;
    }
    handleTopaz(selectedHdr, op.key, params);
  };

  return (
    <div className="topaz-panel">
      <div className="topaz-header">
        <span className="topaz-badge"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><rect x="4" y="4" width="16" height="16" rx="2" /><rect x="9" y="9" width="6" height="6" /><path d="M9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 14h3M1 9h3M1 14h3" /></svg> Topaz AI</span>
        <button
          className="topaz-options-toggle"
          type="button"
          onClick={() => setShowOptions(!showOptions)}
        >
          {showOptions ? "▾ Options" : "▸ Options"}
        </button>
      </div>

      {/* Operation Buttons */}
      <div className="topaz-grid">
        {TOPAZ_OPS.map((op) => (
          <button
            key={op.label}
            className={`topaz-op-btn ${activeOp === op.label && busy ? 'active' : ''}`}
            type="button"
            onClick={() => handleOp(op)}
            disabled={busy}
            title={op.desc}
          >
            <span className="topaz-op-icon">{op.icon}</span>
            <span className="topaz-op-label">{op.label}</span>
          </button>
        ))}
      </div>

      {/* Options Sub-panel */}
      {showOptions && (
        <div className="topaz-options">
          <label className="topaz-option-row">
            <span>Modèle</span>
            <select
              className="topaz-select"
              value={topazModel}
              onChange={(e) => setTopazModel(e.target.value)}
            >
              {TOPAZ_MODELS.enhance.map((m) => (
                <option key={m.value} value={m.value}>{m.label}</option>
              ))}
              <optgroup label="Sharpen">
                {TOPAZ_MODELS.sharpen.map((m) => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </optgroup>
            </select>
          </label>
          <label className="topaz-option-row">
            <span>Scale</span>
            <select
              className="topaz-select"
              value={topazScale}
              onChange={(e) => setTopazScale(e.target.value)}
            >
              <option value="1x">1× (Original)</option>
              <option value="2x">2× Upscale</option>
              <option value="4x">4× Upscale</option>
            </select>
          </label>
          <Slider
            label="Intensité"
            min={0} max={1} step={0.05}
            value={topazStrength}
            defaultValue={0.8}
            onChange={setTopazStrength}
            panel="topaz"
          />
          <label className="topaz-option-row topaz-toggle-row">
            <span>Face Enhancement</span>
            <button
              type="button"
              className={`topaz-toggle ${faceEnhance ? 'on' : ''}`}
              onClick={() => setFaceEnhance(!faceEnhance)}
            >
              <span className="topaz-toggle-knob" />
            </button>
          </label>
        </div>
      )}
    </div>
  );
});

// Histogram Component
const Histogram = React.memo(({ glCanvasRef }) => {
  const canvasRef = useRef(null);

  useEffect(() => {
    const drawHistogram = () => {
      const canvas = canvasRef.current;
      const glCanvas = glCanvasRef?.current;
      if (!canvas || !glCanvas) return;

      const ctx = canvas.getContext('2d');
      const width = canvas.width;
      const height = canvas.height;

      // Clear canvas
      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, width, height);

      try {
        // Sample from WebGL canvas  
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        const sampleWidth = Math.min(glCanvas.width, 256);
        const sampleHeight = Math.min(glCanvas.height, 256);
        tempCanvas.width = sampleWidth;
        tempCanvas.height = sampleHeight;
        tempCtx.drawImage(glCanvas, 0, 0, sampleWidth, sampleHeight);

        const imageData = tempCtx.getImageData(0, 0, sampleWidth, sampleHeight);
        const data = imageData.data;

        // Calculate histograms
        const rHist = new Array(256).fill(0);
        const gHist = new Array(256).fill(0);
        const bHist = new Array(256).fill(0);
        const lumHist = new Array(256).fill(0);

        for (let i = 0; i < data.length; i += 4) {
          const r = data[i];
          const g = data[i + 1];
          const b = data[i + 2];
          rHist[r]++;
          gHist[g]++;
          bHist[b]++;
          const lum = Math.round(0.299 * r + 0.587 * g + 0.114 * b);
          lumHist[lum]++;
        }

        // Find max for scaling
        const maxLum = Math.max(...lumHist);
        const maxRGB = Math.max(...rHist, ...gHist, ...bHist);

        // Draw luminosity histogram (white, filled)
        ctx.globalAlpha = 0.5;
        ctx.fillStyle = '#ffffff';
        ctx.beginPath();
        ctx.moveTo(0, height);
        for (let i = 0; i < 256; i++) {
          const x = (i / 255) * width;
          const h = (lumHist[i] / maxLum) * height;
          ctx.lineTo(x, height - h);
        }
        ctx.lineTo(width, height);
        ctx.closePath();
        ctx.fill();

        // Draw RGB channels
        ctx.globalAlpha = 0.4;
        ctx.lineWidth = 1;

        // Red channel
        ctx.strokeStyle = '#ef4444';
        ctx.beginPath();
        for (let i = 0; i < 256; i++) {
          const x = (i / 255) * width;
          const h = (rHist[i] / maxRGB) * height;
          if (i === 0) ctx.moveTo(x, height - h);
          else ctx.lineTo(x, height - h);
        }
        ctx.stroke();

        // Green channel
        ctx.strokeStyle = '#22c55e';
        ctx.beginPath();
        for (let i = 0; i < 256; i++) {
          const x = (i / 255) * width;
          const h = (gHist[i] / maxRGB) * height;
          if (i === 0) ctx.moveTo(x, height - h);
          else ctx.lineTo(x, height - h);
        }
        ctx.stroke();

        // Blue channel
        ctx.strokeStyle = '#3b82f6';
        ctx.beginPath();
        for (let i = 0; i < 256; i++) {
          const x = (i / 255) * width;
          const h = (bHist[i] / maxRGB) * height;
          if (i === 0) ctx.moveTo(x, height - h);
          else ctx.lineTo(x, height - h);
        }
        ctx.stroke();

      } catch (e) {
        // Silently fail if can't read canvas
        ctx.fillStyle = '#333';
        ctx.fillRect(0, 0, width, height);
        ctx.fillStyle = '#666';
        ctx.font = '10px sans-serif';
        ctx.fillText('No data', 10, height / 2);
      }
    };

    // Redraw on animation frame for smooth updates
    const timer = setInterval(drawHistogram, 200);
    drawHistogram();

    return () => clearInterval(timer);
  }, [glCanvasRef]);

  return (
    <div className="histogram-container">
      <canvas ref={canvasRef} width={256} height={60} className="histogram-canvas" />
    </div>
  );
});


const PreviewSection = React.memo(({
  selectedHdr, renderMode, renderNote, rotation, setRotation, result, setResult,
  glCanvasRef, glReady, localPreview, preview
}) => {
  // Zoom and pan state
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });
  const [isDragging, setIsDragging] = useState(false);
  const dragRef = useRef({ startX: 0, startY: 0, panX: 0, panY: 0 });
  const containerRef = useRef(null);
  const [containerSize, setContainerSize] = useState({ w: 0, h: 0 });

  // Measure container for dimension-based zoom
  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const ro = new ResizeObserver((entries) => {
      const { width, height } = entries[0].contentRect;
      setContainerSize({ w: width, h: height });
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, []);

  // Compute canvas display dimensions
  const canvasBufW = glCanvasRef.current?.width || 1;
  const canvasBufH = glCanvasRef.current?.height || 1;
  const aspect = canvasBufW / canvasBufH;

  let fitW, fitH;
  if (containerSize.w && containerSize.h) {
    if (containerSize.w / containerSize.h > aspect) {
      fitH = containerSize.h;
      fitW = fitH * aspect;
    } else {
      fitW = containerSize.w;
      fitH = fitW / aspect;
    }
  } else {
    fitW = canvasBufW;
    fitH = canvasBufH;
  }

  const displayW = fitW * zoom;
  const displayH = fitH * zoom;

  // Center offset when image is smaller than container
  const offsetX = (containerSize.w - displayW) / 2 + pan.x;
  const offsetY = (containerSize.h - displayH) / 2 + pan.y;

  // Reset zoom/pan when image changes
  useEffect(() => {
    setZoom(1);
    setPan({ x: 0, y: 0 });
  }, [selectedHdr]);

  // Auto-dismiss result popup after 5 seconds
  useEffect(() => {
    if (result) {
      const timer = setTimeout(() => {
        setResult(null);
      }, 5000);
      return () => clearTimeout(timer);
    }
  }, [result, setResult]);

  // Wheel zoom — passive:false to allow preventDefault
  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;

    const onWheel = (e) => {
      e.preventDefault();
      e.stopPropagation();

      if (e.ctrlKey) {
        // Trackpad pinch-zoom (Ctrl+wheel) — use fine delta
        const pinchDelta = -e.deltaY * 0.01;
        setZoom((z) => Math.min(Math.max(z * (1 + pinchDelta), 0.1), 20));
      } else {
        // Regular mouse wheel or trackpad two-finger scroll
        setZoom((prevZoom) => {
          if (prevZoom > 1.05) {
            // Pan when zoomed in with scroll
            setPan((p) => ({
              x: p.x - e.deltaX,
              y: p.y - e.deltaY,
            }));
            return prevZoom;
          }
          // Otherwise zoom
          const delta = e.deltaY > 0 ? 0.92 : 1.08;
          return Math.min(Math.max(prevZoom * delta, 0.1), 20);
        });
      }
    };

    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);

  // Pointer drag — works on trackpad, mouse, and touch
  const handlePointerDown = useCallback((e) => {
    if (e.button !== 0) return;
    setIsDragging(true);
    dragRef.current = {
      startX: e.clientX,
      startY: e.clientY,
      panX: pan.x,
      panY: pan.y,
    };
    e.currentTarget.setPointerCapture(e.pointerId);
  }, [pan]);

  const handlePointerMove = useCallback((e) => {
    if (!isDragging) return;
    const dx = e.clientX - dragRef.current.startX;
    const dy = e.clientY - dragRef.current.startY;
    setPan({
      x: dragRef.current.panX + dx,
      y: dragRef.current.panY + dy,
    });
  }, [isDragging]);

  const handlePointerUp = useCallback(() => {
    setIsDragging(false);
  }, []);

  // Zoom controls - with stopPropagation to prevent drag
  const zoomIn = (e) => { e.stopPropagation(); setZoom((z) => Math.min(z * 1.25, 20)); };
  const zoomOut = (e) => { e.stopPropagation(); setZoom((z) => Math.max(z / 1.25, 0.1)); };
  const zoomFit = (e) => { e.stopPropagation(); setZoom(1); setPan({ x: 0, y: 0 }); };
  const zoom100 = (e) => {
    e.stopPropagation();
    // 1:1 pixel mapping — canvas buffer pixels = CSS pixels
    if (containerSize.w && fitW) {
      setZoom(canvasBufW / fitW);
    }
    setPan({ x: 0, y: 0 });
  };

  return (
    <div
      className="hdr-preview"
      ref={containerRef}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
      style={{ cursor: isDragging ? 'grabbing' : 'grab', overflow: 'hidden', touchAction: 'none', position: 'relative' }}
    >
      {/* Zoom Controls Toolbar */}
      <div className="zoom-toolbar" onMouseDown={(e) => e.stopPropagation()}>
        <button className="zoom-btn" onClick={zoomOut} title="Zoom -">−</button>
        <span className="zoom-level">{Math.round(zoom * 100)}%</span>
        <button className="zoom-btn" onClick={zoomIn} title="Zoom +">+</button>
        <button className="zoom-btn" onClick={zoomFit} title="Ajuster">Fit</button>
        <button className="zoom-btn" onClick={zoom100} title="100%">1:1</button>
      </div>

      <div>
        <div style={{ display: "flex", gap: 8, alignItems: "center", position: 'absolute', top: 10, right: 10, zIndex: 10 }}>
          {selectedHdr && (
            <div className="mode-info">
              <span
                className={`badge mode-badge ${renderMode === "WEBGL" ? "gpu" : renderMode === "CPU" ? "cpu" : "loading"}`}
                title={renderNote}
              >
                {renderMode}
              </span>
              {renderNote && <span className="mode-note">{renderNote}</span>}
            </div>
          )}
          {selectedHdr && (
            <button
              className="icon-btn rotate-btn"
              type="button"
              onClick={() => setRotation((r) => (r === 0 ? 180 : 0))}
              title="Rotation 180"
            >
              Rotate
            </button>
          )}
          {result && <div className="badge" style={{ background: "rgba(255,255,255,0.06)" }}>{result.output_path.split(/[/\\]/).pop()}</div>}
        </div>
      </div>
      {result && (
        <div className="result-glass-card">
          <button
            className="icon-btn result-close-btn"
            title="Fermer"
            onClick={() => setResult(null)}
            style={{ position: 'absolute', top: 8, right: 8 }}
          >
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <path d="M18 6L6 18M6 6l12 12"></path>
            </svg>
          </button>
          <div className="result-icon-area">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" style={{ color: '#10b981' }}>
              <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path>
              <polyline points="22 4 12 14.01 9 11.01"></polyline>
            </svg>
          </div>
          <div className="result-info">
            <div className="result-title">Fusion terminée avec succès</div>
            <div className="result-filename" title={result.output_path}>
              {result.output_path.split(/[/\\]/).pop()}
            </div>
            <div className="result-path mono-ellipsis">{result.output_path}</div>
          </div>
          <button className="icon-btn" title="Copier le chemin" onClick={() => navigator.clipboard.writeText(result.output_path)}>
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
              <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
            </svg>
          </button>
        </div>
      )}
      {selectedHdr ? (
        <div className="preview-frame" style={{ position: 'relative', overflow: 'hidden' }}>
          <canvas
            ref={glCanvasRef}
            className={`preview-canvas ${rotation ? "rot-180" : ""}`}
            style={{
              display: 'block',
              position: 'absolute',
              left: `${offsetX}px`,
              top: `${offsetY}px`,
              width: `${displayW}px`,
              height: `${displayH}px`,
              maxWidth: 'none',
              maxHeight: 'none',
              imageRendering: zoom > 2 ? 'pixelated' : 'auto',
            }}
          />
          {!glReady && !localPreview && (
            <div style={{ position: "absolute", inset: 0, display: "flex", alignItems: "center", justifyContent: "center", color: "#93a4bf", fontSize: 13 }}>
              Chargement de la preview...
            </div>
          )}
          {!glReady && localPreview && (
            <img
              src={localPreview}
              alt="Preview CPU"

              className={`preview-fallback ${rotation ? "rot-180" : ""}`}
            />
          )}
        </div>
      ) : preview ? (
        <img src={preview} alt="HDR preview large" />
      ) : (
        <div className="preview-empty"></div>
      )}
    </div>
  );
});

const LeftPanelBody = React.memo(({
  selectedHdr, error, message, handleSubmit, missionId, setMissionId, loadGroups, settings,
  bracketPathsText, triggerPreview, missions, outputName, setOutputName, setBracketPathsText,
  busy, groups, loadingGroups, useGroup, renderGroupThumb, hdrFiles, apiBase,
  setSelectedHdr, setPreview, setLocalPreview, loadSelectedHdrImage, setMessage
}) => {
  return (
    <div className="hdr-drawer-content">
      {selectedHdr && (
        <div className="badge active" style={{ marginBottom: 16 }}>
          En cours : {selectedHdr.name}
        </div>
      )}
      {error && <div className="badge error" style={{ marginBottom: 16 }}>{error}</div>}
      {message && <div className="badge success" style={{ marginBottom: 16 }}>{message}</div>}

      <div className="drawer-section">
        <label className="drawer-label">Mission</label>
        <select
          className="field"
          value={missionId}
          onChange={(e) => {
            const mid = e.target.value;
            setMissionId(mid);
            loadGroups(mid);
            triggerPreview(settings, mid, bracketPathsText);
          }}
        >
          <option value="" disabled>Choisir une mission...</option>
          {missions.map((m) => (
            <option key={m.id} value={m.id}>
              {m.title}
            </option>
          ))}
        </select>
      </div>

      <div className="drawer-section">
        <label className="drawer-label">Fichier</label>
        <input
          className="field"
          value={outputName}
          onChange={(e) => setOutputName(e.target.value)}
          placeholder="hdr_output"
        />
      </div>

      <div className="drawer-section">
        <label className="drawer-label">Sources (Brackets)</label>
        <textarea
          className="paths-area"
          rows={5}
          value={bracketPathsText}
          onChange={(e) => {
            const val = e.target.value;
            setBracketPathsText(val);
            triggerPreview(settings, missionId, val);
          }}
          placeholder="Chemins des fichiers..."
          spellCheck={false}
        />
      </div>

      <button className="btn full-width" onClick={handleSubmit} disabled={busy}>
        {busy ? "Traitement..." : "Fusionner HDR"}
      </button>

      <div className="divider" style={{ margin: '8px 0', borderColor: 'var(--border-subtle)', borderBottomWidth: 1 }} />

      {Object.keys(groups).length > 0 && (
        <div className="drawer-section">
          <label className="drawer-label">Groupes ({Object.keys(groups).length})</label>
          <div className="compact-list">
            {Object.entries(groups).map(([name, paths]) => (
              <div key={name} className="compact-item" onClick={() => useGroup(name, paths)}>
                <div className="compact-thumb-wrap">
                  {renderGroupThumb(name, hdrFiles, apiBase)}
                </div>
                <div className="compact-info">
                  <div className="compact-title">{name}</div>
                  <div className="compact-sub">{paths.length} photos</div>
                </div>
                <button className="icon-btn" title="Utiliser">
                  <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M5 12h14M12 5l7 7-7 7" /></svg>
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {loadingGroups && <div className="text-muted">Chargement...</div>}

      {hdrFiles.length > 0 && (
        <div className="drawer-section">
          <label className="drawer-label">Resultats ({hdrFiles.length})</label>
          <div className="compact-list">
            {hdrFiles.map((hdr) => (
              <div key={hdr.url} className={`compact-item ${selectedHdr?.url === hdr.url ? "active" : ""}`} onClick={() => {
                setSelectedHdr(hdr);
                setPreview(`${apiBase}${hdr.url}`);
                setLocalPreview("");
                setOutputName(hdr.name.replace(/\.[^.]+$/, "") + "_edit");
                setMessage(`Selectionne: ${hdr.name}`);
                loadSelectedHdrImage(hdr);
              }}>
                <img
                  src={`${apiBase}${hdr.url}`}
                  alt={hdr.name}
                  className="compact-thumb"
                  loading="lazy"
                  decoding="async"
                />
                <div className="compact-info">
                  <div className="compact-title">{hdr.name}</div>
                  <div className="compact-sub">{hdr.path.split(/[/\\]/).slice(-2).join("/")}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
});
