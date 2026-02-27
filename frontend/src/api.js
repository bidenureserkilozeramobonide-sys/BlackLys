import axios from "axios";

export const API_BASE = import.meta.env.VITE_API_BASE || "http://127.0.0.1:8001";

export const api = axios.create({
  baseURL: API_BASE,
  timeout: 15000, // P9: 15s timeout — prevent hung backend from freezing UI
});

const pickError = (err) => {
  if (err?.response?.data?.detail) return err.response.data.detail;
  if (err?.message) return err.message;
  return "Erreur réseau";
};

// --- Stats ---
export async function getDashboardStats() {
  try {
    const { data } = await api.get("/stats/dashboard");
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

// --- Clients ---
export async function listClients() {
  try {
    const { data } = await api.get("/clients/");
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function createClient(payload) {
  try {
    const { data } = await api.post("/clients/", payload);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function updateClient(clientId, payload) {
  try {
    const { data } = await api.put(`/clients/${clientId}`, payload);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function deleteClient(clientId) {
  try {
    await api.delete(`/clients/${clientId}`);
  } catch (err) {
    throw new Error(pickError(err));
  }
}

// --- Missions ---
export async function listMissions() {
  try {
    const { data } = await api.get("/missions/");
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function createMission(payload) {
  try {
    const { data } = await api.post("/missions/", payload);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function uploadRaw(missionId, files) {
  const form = new FormData();
  files.forEach((file) => form.append("files", file));
  try {
    const { data } = await api.post(`/missions/${missionId}/raw`, form, {
      headers: { "Content-Type": "multipart/form-data" },
    });
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function groupBrackets(missionId, opts) {
  try {
    const { data } = await api.post(`/missions/${missionId}/group`, opts);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function processHDR({ missionId, bracketPaths, settings, outputName }) {
  try {
    const { data } = await api.post("/hdr/process", {
      mission_id: missionId,
      bracket_paths: bracketPaths,
      output_name: outputName,
      settings,
    });
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

// --- Galleries ---
export async function listGalleries() {
  try {
    const { data } = await api.get("/galleries/");
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function createGallery(missionId, payload) {
  try {
    const { data } = await api.post(`/galleries/missions/${missionId}`, payload);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function deleteGallery(missionId) {
  try {
    await api.delete(`/galleries/missions/${missionId}`);
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function getGallery(missionId) {
  try {
    const { data } = await api.get(`/galleries/missions/${missionId}`);
    return data;
  } catch (err) {
    if (err?.response?.status === 404) return null;
    throw new Error(pickError(err));
  }
}

export async function getMissionGroups(missionId) {
  try {
    const { data } = await api.get(`/missions/${missionId}/groups`);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function getMissionHdrs(missionId) {
  try {
    const { data } = await api.get(`/missions/${missionId}/hdrs`);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function adjustHDR({ missionId, inputFilename, settings, outputName }) {
  try {
    const { data } = await api.post("/hdr/adjust", {
      mission_id: missionId,
      input_filename: inputFilename,
      output_name: outputName,
      settings,
    });
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

// --- Invoices ---
export async function listInvoices() {
  try {
    const { data } = await api.get("/invoices/");
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

export async function createInvoice(missionId, payload) {
  try {
    const { data } = await api.post(`/invoices/missions/${missionId}`, payload);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}

// --- Topaz ---
export async function enhanceWithTopaz({ path, model, outputName, strength, scale_factor, face_enhancement, fix_compression }) {
  try {
    const body = {
      file_path: path,
      model,
      output_name: outputName,
    };
    if (strength !== undefined) body.strength = strength;
    if (scale_factor !== undefined) body.scale_factor = scale_factor;
    if (face_enhancement !== undefined) body.face_enhancement = face_enhancement;
    if (fix_compression !== undefined) body.fix_compression = fix_compression;
    const { data } = await api.post("/topaz/enhance", body);
    return data;
  } catch (err) {
    throw new Error(pickError(err));
  }
}
