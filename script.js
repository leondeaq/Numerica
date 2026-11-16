/**
 * NUMERICA - Calculadora de Métodos Numéricos
 * JavaScript principal para manejo de la interfaz y puente con WebAssembly (Emscripten / C++)
 */

// VARIABLES GLOBALES DE ESTADO
let metodoActivo = 'newton';
let unidadActiva = 'unidad2';

// 1. UTILIDADES Y UI
function insertarSimbolo(simbolo) {
  const input = document.getElementById('inputFuncion');
  const start = input.selectionStart;
  const end = input.selectionEnd;
  input.value = input.value.substring(0, start) + simbolo + input.value.substring(end);
  input.focus();
}

function confirmarSalida() {
  if (confirm("¿Estás seguro de que deseas salir de Numérica?")) window.close();
}

function mostrarSeccion(idUnidad, btn) {
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  if(btn) btn.classList.add('active');
  
  unidadActiva = idUnidad;
  document.getElementById('unidad2').classList.add('seccion-oculta');
  document.getElementById('unidadMatricial').classList.add('seccion-oculta');

  if (idUnidad === 'unidad2') {
    document.getElementById('unidad2').classList.remove('seccion-oculta');
    seleccionarMetodo('newton', 'unidad2');
  } else {
    document.getElementById('unidadMatricial').classList.remove('seccion-oculta');
    configurarSidebarMatricial(idUnidad);
  }
}

function configurarSidebarMatricial(unidad) {
  const lista = document.getElementById('listaMetodosMatriciales');
  const titulo = document.getElementById('tituloSidebarMatricial');
  lista.innerHTML = '';
  
  let metodos = [];
  if(unidad === 'unidad3') {
    titulo.innerText = "Sistemas Directos";
    metodos = [
      { id: 'gaussjordan', nombre: 'Gauss-Jordan' },
      { id: 'doolittle', nombre: 'Factorización LU (Doolittle)' },
      { id: 'crout', nombre: 'Factorización LU (Crout)' }
    ];
  } else if(unidad === 'unidad4') {
    titulo.innerText = "Métodos Iterativos";
    metodos = [
      { id: 'jacobi', nombre: 'Jacobi' },
      { id: 'gaussseidel', nombre: 'Gauss-Seidel' }
    ];
  } else if(unidad === 'unidad5') {
    titulo.innerText = "Valores Propios";
    metodos = [
      { id: 'potencia', nombre: 'Método de la Potencia' },
      { id: 'potenciainversa', nombre: 'Potencia Inversa' }
    ];
  }
  
  metodos.forEach((m, idx) => {
    lista.innerHTML += `<li class="method-item ${idx===0?'active':''}"><a href="#" onclick="seleccionarMetodo('${m.id}', '${unidad}', this.parentElement)">${m.nombre}</a></li>`;
  });
  
  seleccionarMetodo(metodos[0].id, unidad);
}

function seleccionarMetodo(metodo, unidad, liElement) {
  metodoActivo = metodo;
  
  if(liElement) {
    liElement.parentElement.querySelectorAll('.method-item').forEach(el => el.classList.remove('active'));
    liElement.classList.add('active');
  }

  if (unidad === 'unidad2') {
    document.getElementById('tituloMetodoU2').innerText = "Método de " + metodo.toUpperCase();
    document.getElementById('grpX1').style.display = (metodo === 'biseccion' || metodo === 'secante') ? 'block' : 'none';
  } else {
    document.getElementById('tituloMetodoMatricial').innerText = "Método: " + metodo.toUpperCase();
    document.getElementById('grpTolMat').style.display = (metodo === 'gaussjordan' || metodo === 'doolittle' || metodo === 'crout') ? 'none' : 'block';
    document.getElementById('labelVectorB').innerText = (metodo === 'potencia' || metodo === 'potenciainversa') ? 'Vector inicial x0' : 'Vector b';
    generarGrillasMatriciales();
  }
  
  document.getElementById('cardResultadosU2').classList.add('hidden');
  document.getElementById('cardResultadosMatricial').classList.add('hidden');
}

// 2. GENERADORES DE GRILLA (UI)
function inicializarGrillas() { generarGrillasMatriciales(); }

function generarGrillasMatriciales() {
  const n = parseInt(document.getElementById('inputN').value) || 3;
  const gridA = document.getElementById('gridMatrizA');
  const gridB = document.getElementById('gridVectorB');
  
  gridA.style.gridTemplateColumns = `repeat(${n}, 1fr)`;
  gridA.innerHTML = '';
  for(let i=0; i<n; i++) {
    for(let j=0; j<n; j++) {
      gridA.innerHTML += `<input type="number" step="any" class="matrix-input" id="A_${i}_${j}" value="${i===j?2:1}">`;
    }
  }
  
  gridB.innerHTML = '';
  for(let i=0; i<n; i++) {
    gridB.innerHTML += `<input type="number" step="any" class="matrix-input vector-input" id="B_${i}" value="1">`;
  }
}

// 3. PARSERS PARA C++ VÍA EMBIND
function leerMatrizC(n) {
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

function leerVectorC(n) {
  let vec = new Module.VectorDouble();
  for(let i=0; i<n; i++) {
    vec.push_back(parseFloat(document.getElementById(`B_${i}`).value) || 0);
  }
  return vec;
}

// 4. EJECUTORES
function ejecutarMetodoU2() {
  if (typeof Module === 'undefined') return alert("Cargando módulo de C++ (WebAssembly)...");
  
  const f = document.getElementById('inputFuncion').value;
  const x0 = parseFloat(document.getElementById('inputX0').value);
  const x1 = parseFloat(document.getElementById('inputX1').value) || 0;
  const tol = parseFloat(document.getElementById('inputTol').value);
  const maxI = parseInt(document.getElementById('inputMaxIter').value);
  
  let res;
  try {
    if (metodoActivo === 'newton') res = Module.calcular_newton_cpp(f, x0, tol, maxI);
    else if (metodoActivo === 'biseccion') res = Module.calcular_biseccion_cpp(f, x0, x1, tol, maxI);
    else if (metodoActivo === 'secante') res = Module.calcular_secante_cpp(f, x0, x1, tol, maxI);
    
    renderResultadosU2(res, metodoActivo);
  } finally {
    if (res && res.tabla) res.tabla.delete();
  }
}

function renderResultadosU2(res, metodo) {
  const card = document.getElementById('cardResultadosU2');
  const errDiv = document.getElementById('alertaErrorU2');
  card.classList.remove('hidden');
  
  if (!res.exito) {
    errDiv.innerText = res.error_msg;
    errDiv.classList.remove('hidden');
    return;
  }
  
  errDiv.classList.add('hidden');
  document.getElementById('valRaizU2').innerText = res.raiz.toFixed(6);
  document.getElementById('valIterU2').innerText = res.iteraciones;
  
  let html = '';
  for(let i=0; i<res.tabla.size(); i++) {
    const fila = res.tabla.get(i);
    html += `<tr><td>${fila.k}</td><td>${fila.xk !== undefined ? fila.xk.toFixed(6) : fila.xr.toFixed(6)}</td><td>${fila.error.toExponential(4)}</td></tr>`;
  }
  document.getElementById('headTablaU2').innerHTML = '<tr><th>k</th><th>x</th><th>Error</th></tr>';
  document.getElementById('cuerpoTablaU2').innerHTML = html;
}

function ejecutarMetodoMatricial() {
  if (typeof Module === 'undefined') return alert("Cargando módulo de C++ (WebAssembly)...");

  const n = parseInt(document.getElementById('inputN').value);
  const tol = parseFloat(document.getElementById('inputTolMat').value) || 0.0001;
  const maxI = 100;
  
  const A = leerMatrizC(n);
  const b = leerVectorC(n);
  
  let res;
  try {
    if (metodoActivo === 'gaussjordan') res = Module.calcular_gauss_jordan_cpp(A, b);
    else if (metodoActivo === 'doolittle') res = Module.calcular_doolittle_cpp(A, b);
    else if (metodoActivo === 'crout') res = Module.calcular_crout_cpp(A, b);
    else if (metodoActivo === 'jacobi') res = Module.calcular_jacobi_cpp(A, b, b, tol, maxI);
    else if (metodoActivo === 'gaussseidel') res = Module.calcular_gauss_seidel_cpp(A, b, b, tol, maxI);
    else if (metodoActivo === 'potencia') res = Module.calcular_potencia_cpp(A, b, tol, maxI);
    else if (metodoActivo === 'potenciainversa') res = Module.calcular_potencia_inversa_cpp(A, b, tol, maxI);
    
    renderResultadosMatriciales(res, n);
  } finally {
    A.delete();
    b.delete();
    // Limpiar memoria de vectores retornados por las estructuras C++
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

function renderResultadosMatriciales(res, n) {
  const card = document.getElementById('cardResultadosMatricial');
  const errDiv = document.getElementById('alertaErrorMat');
  const din = document.getElementById('resultadosDinamicosMatriciales');
  
  card.classList.remove('hidden');
  if (!res.exito) {
    errDiv.innerText = res.error_msg;
    errDiv.classList.remove('hidden');
    din.innerHTML = '';
    return;
  }
  errDiv.classList.add('hidden');
  
  let html = `<h4>Método ejecutado exitosamente</h4>`;
  
  // Extraer vector solución general (Gauss, LU, Iterativos)
  if(res.x || res.x_solucion || res.v_propio) {
    let vec = res.x || res.x_solucion || res.v_propio;
    html += `<div class="matrix-bracket-small">X = [`;
    for(let i=0; i<vec.size(); i++) html += `<div class="vector-val">${vec.get(i).toFixed(6)}</div>`;
    html += `]</div>`;
  }

  // Extraer Eigenvalor
  if(res.lambda_max !== undefined) html += `<p>Eigenvalor dominante: <strong>${res.lambda_max.toFixed(6)}</strong></p>`;
  if(res.lambda_min !== undefined) html += `<p>Eigenvalor mínimo: <strong>${res.lambda_min.toFixed(6)}</strong></p>`;

  din.innerHTML = html;
}
