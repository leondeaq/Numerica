/**
 * NUMERICA - Calculadora de Métodos Numéricos
 * JavaScript principal para manejo de la interfaz y puente con WebAssembly (Emscripten / C++)
 */

let unidadActiva = 'unidad2';
let metodoActivo = 'biseccion';

// ==========================================
// 1. NAVEGACIÓN Y UI
// ==========================================

// Cambiar entre unidades desde el menú superior
function mostrarUnidad(idUnidad, btn) {
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  if(btn) btn.classList.add('active');
  
  ['unidad2', 'unidad3', 'unidad4', 'unidad5'].forEach(u => {
    document.getElementById(u).classList.add('hidden');
  });
  
  document.getElementById(idUnidad).classList.remove('hidden');
  unidadActiva = idUnidad;

  const primerEnlace = document.querySelector(`#${idUnidad} .method-list .method-item a`);
  if (primerEnlace) primerEnlace.click();
  
  if(idUnidad !== 'unidad2') {
    // Usamos tu selector original que ya funcionaba perfecto
    const formCard = document.querySelector('.form-card:has(#dimN)');
    const resCard = document.querySelector('#resMatricial');
    const contenedor = document.querySelector(`#${idUnidad} .content-area`);
    
    // Lo movemos al final del contenedor actual
    if(formCard && resCard) {
      contenedor.appendChild(formCard);
      contenedor.appendChild(resCard);
    }
    
    generarGrillas();
  }
  
}

// Cambiar entre métodos desde el menú lateral
function cambiarVista(unidad, metodo, elementoLi) {
  metodoActivo = metodo;

  const lista = elementoLi.closest('.method-list');
  lista.querySelectorAll('.method-item').forEach(li => li.classList.remove('active'));
  elementoLi.parentElement.classList.add('active');

  document.querySelectorAll(`#${unidad} .vista-metodo`).forEach(v => v.classList.add('hidden'));
  document.getElementById(`vista-${metodo}`).classList.remove('hidden');

  document.getElementById('resU2')?.classList.add('hidden');
  document.getElementById('resMatricial')?.classList.add('hidden');

  // Ajustes de UI para matrices según el método
  if (unidad !== 'unidad2') {
    const grpTol = document.getElementById('grpTolMat');
    if (grpTol) {
      grpTol.style.display = ['jacobi', 'gaussseidel', 'potencia', 'potenciainversa'].includes(metodo) ? 'block' : 'none';
    }
  }
}

function confirmarSalida() {
  if (confirm("¿Estás seguro de que deseas salir de Numérica?")) window.close();
}

// ==========================================
// 2. UTILIDADES PARA MATRICES (U3, U4, U5)
// ==========================================

function generarGrillas() {
  const n = parseInt(document.getElementById('dimN').value) || 3;
  const gridA = document.getElementById('gridA');
  const gridB = document.getElementById('gridB');
  
  gridA.style.gridTemplateColumns = `repeat(${n}, 1fr)`;
  gridA.innerHTML = '';
  gridB.innerHTML = '';

  for(let i=0; i<n; i++) {
    for(let j=0; j<n; j++) {
      gridA.innerHTML += `<input type="number" step="any" class="matrix-input" id="A_${i}_${j}" value="${i===j?2:1}">`;
    }
    gridB.innerHTML += `<input type="number" step="any" class="matrix-input vector-input" id="B_${i}" value="1">`;
  }
}

function leerMatriz(n) {
  let mat = new Module.VectorVectorDouble();
  for(let i=0; i<n; i++) {
    let row = new Module.VectorDouble();
    for(let j=0; j<n; j++) {
      row.push_back(parseFloat(document.getElementById(`A_${i}_${j}`).value) || 0);
    }
    mat.push_back(row);
  }
  return mat;
}

function leerVector(n) {
  let vec = new Module.VectorDouble();
  for(let i=0; i<n; i++) {
    vec.push_back(parseFloat(document.getElementById(`B_${i}`).value) || 0);
  }
  return vec;
}

// ==========================================
// 3. EJECUTORES DE UNIDAD 2 (Raíces)
// ==========================================

function ejecutarU2(metodo) {
  if (typeof Module === 'undefined') return alert("Cargando módulo...");
  
  let res;
  try {
    if (metodo === 'newton') {
      const f = document.getElementById('fn_newton').value;
      const x0 = parseFloat(document.getElementById('x0_newton').value);
      const tol = parseFloat(document.getElementById('tol_newton').value);
      const iter = parseInt(document.getElementById('iter_newton').value);
      res = Module.calcular_newton_cpp(f, x0, tol, iter);
    } 
    else if (metodo === 'biseccion') {
      const f = document.getElementById('fn_biseccion').value;
      const a = parseFloat(document.getElementById('a_bis').value);
      const b = parseFloat(document.getElementById('b_bis').value);
      const tol = parseFloat(document.getElementById('tol_bis').value);
      const iter = parseInt(document.getElementById('iter_bis').value);
      res = Module.calcular_biseccion_cpp(f, a, b, tol, iter);
    }
    else if (metodo === 'secante') {
      const f = document.getElementById('fn_secante').value;
      const x0 = parseFloat(document.getElementById('x0_sec').value);
      const x1 = parseFloat(document.getElementById('x1_sec').value);
      const tol = parseFloat(document.getElementById('tol_sec').value);
      const iter = parseInt(document.getElementById('iter_sec').value);
      res = Module.calcular_secante_cpp(f, x0, x1, tol, iter);
    }
    renderResultadosU2(res);
  } catch (e) {
    console.error("Bug detectado:", e);
    mostrarErrorU2("Error en la ejecución. Revisa los datos de entrada.");
  } finally {
    if (res && res.tabla) res.tabla.delete();
  }
}

function mostrarErrorU2(msg) {
  document.getElementById('resU2').classList.remove('hidden');
  document.getElementById('errU2').innerText = msg;
  document.getElementById('errU2').classList.remove('hidden');
  document.querySelector('#resU2 .metrics-grid').style.display = 'none';
  document.querySelector('#resU2 .table-container').style.display = 'none';
}

function renderResultadosU2(res) {
  document.getElementById('resU2').classList.remove('hidden');
  
  if (!res.exito) {
    mostrarErrorU2(res.error_msg || "El método falló.");
    return;
  }
  
  document.getElementById('errU2').classList.add('hidden');
  document.querySelector('#resU2 .metrics-grid').style.display = 'grid';
  document.querySelector('#resU2 .table-container').style.display = 'block';

  document.getElementById('valRaizU2').innerText = res.raiz.toFixed(6);
  document.getElementById('valIterU2').innerText = res.iteraciones;
  
  let html = '';
  for(let i=0; i<res.tabla.size(); i++) {
    const fila = res.tabla.get(i);
    const x_val = fila.xk !== undefined ? fila.xk : fila.xr;
    html += `<tr><td>${fila.k}</td><td>${x_val.toFixed(6)}</td><td>${fila.error.toExponential(4)}</td></tr>`;
  }
  document.getElementById('tablaU2').innerHTML = html;
}

// ==========================================
// 4. EJECUTORES MATRICIALES (U3, U4, U5)
// ==========================================

function ejecutarMatricial() {
  if (typeof Module === 'undefined') return alert("Cargando módulo...");

  const n = parseInt(document.getElementById('dimN').value);
  const tol = parseFloat(document.getElementById('tolMat').value) || 0.0001;
  const maxI = 100;
  
  const A = leerMatriz(n);
  const b = leerVector(n);
  
  let res;
  try {
    if (metodoActivo === 'gaussjordan') res = Module.calcular_gauss_jordan_cpp(A, b);
    else if (metodoActivo === 'doolittle') res = Module.calcular_doolittle_cpp(A, b);
    else if (metodoActivo === 'crout') res = Module.calcular_crout_cpp(A, b);
    else if (metodoActivo === 'jacobi') res = Module.calcular_jacobi_cpp(A, b, b, tol, maxI);
    else if (metodoActivo === 'gaussseidel') res = Module.calcular_gauss_seidel_cpp(A, b, b, tol, maxI);
    else if (metodoActivo === 'potencia') res = Module.calcular_potencia_cpp(A, b, tol, maxI);
    else if (metodoActivo === 'potenciainversa') res = Module.calcular_potencia_inversa_cpp(A, b, tol, maxI);
    
    renderMatricial(res);
  } finally {
    A.delete();
    b.delete();
    if (res && res.x) res.x.delete();
    if (res && res.pasos) res.pasos.delete();
    if (res && res.tabla) res.tabla.delete();
    if (res && res.L) res.L.delete();
    if (res && res.U) res.U.delete();
    if (res && res.y) res.y.delete();
    if (res && res.v_propio) res.v_propio.delete();
    if (res && res.x_solucion) res.x_solucion.delete();
  }
}

function renderMatricial(res) {
  const card = document.getElementById('resMatricial');
  const err = document.getElementById('errMatricial');
  const sal = document.getElementById('salidaMatricial');
  
  card.classList.remove('hidden');
  if (!res.exito) {
    err.innerText = res.error_msg || "Error en el cálculo.";
    err.classList.remove('hidden');
    sal.innerHTML = '';
    return;
  }
  err.classList.add('hidden');
  
  let html = `<p class="success-text">Cálculo exitoso.</p>`;
  
  let vecSol = res.x || res.x_solucion || res.v_propio;
  if(vecSol) {
    html += `<h4>Vector Solución</h4><div class="matrix-bracket-small">[ `;
    for(let i=0; i<vecSol.size(); i++) html += `<span class="vector-val">${vecSol.get(i).toFixed(6)}</span> `;
    html += `]</div>`;
  }

  if(res.lambda_max !== undefined) html += `<h4>Eigenvalor dominante:</h4> <p>${res.lambda_max.toFixed(6)}</p>`;
  if(res.lambda_min !== undefined) html += `<h4>Eigenvalor mínimo:</h4> <p>${res.lambda_min.toFixed(6)}</p>`;

  sal.innerHTML = html;
}
