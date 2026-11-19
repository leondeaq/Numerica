/**
 * NUMERICA - Calculadora de Métodos Numéricos
 * JavaScript principal para manejo de la interfaz y puente con WebAssembly (Emscripten / C++)
 */

let unidadActiva = 'inicio';
let metodoActivo = 'bienvenida';

// ==========================================
// 1. NAVEGACIÓN Y UI
// ==========================================

// Cambiar entre unidades desde el menú superior
function mostrarUnidad(idUnidad, btn) {
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  if(btn) btn.classList.add('active');
  
  ['inicio', 'unidad2', 'unidad3', 'unidad4', 'unidad5'].forEach(u => {
    document.getElementById(u).classList.add('hidden');
  });
  
  document.getElementById(idUnidad).classList.remove('hidden');
  unidadActiva = idUnidad;

  const primerEnlace = document.querySelector(`#${idUnidad} .method-list .method-item a`);
  if (primerEnlace) primerEnlace.click();
  
  // Mover el formulario matricial SOLO si es la unidad 3, 4 o 5
  if(['unidad3', 'unidad4', 'unidad5'].includes(idUnidad)) {
    const formCard = document.querySelector('.form-card:has(#dimN)');
    const resCard = document.querySelector('#resMatricial');
    const contenedor = document.querySelector(`#${idUnidad} .content-area`);
    
    // Lo movemos al final del contenedor actual
    if(formCard && resCard) {
      contenedor.appendChild(formCard);
      contenedor.appendChild(resCard);
    }
    
    // Cambiar el texto de la etiqueta del Vector dinámicamente
    const lblVector = document.getElementById('lblVector');
    if (lblVector) {
      lblVector.innerText = (idUnidad === 'unidad5') ? 'Vector x0' : 'Vector b';
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
  if (unidad !== 'unidad2' && unidad !== 'inicio') {
    const formMatricial = document.querySelector('.form-card:has(#dimN)');
    
    // Si estamos en una pestaña de introducción, ocultamos la calculadora de matrices
    if (metodo.startsWith('intro-')) {
      if (formMatricial) formMatricial.style.display = 'none';
    } else {
      if (formMatricial) formMatricial.style.display = 'block';
      
      // Mostrar/ocultar tolerancia solo para métodos iterativos
      const grpTol = document.getElementById('grpTolMat');
      if (grpTol) {
        grpTol.style.display = ['jacobi', 'gaussseidel', 'potencia', 'potenciainversa'].includes(metodo) ? 'block' : 'none';
      }
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
  const inputN = document.getElementById('dimN');
  let n = parseInt(inputN.value);

  // Validar antes de dibujar la cuadrícula
  if (isNaN(n) || n < 2 || n > 10) {
    alert("Por favor, ingresa una dimensión válida entre 2 y 10.");
    n = 3; // Restaurar a un valor seguro
    inputN.value = 3;
  }

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
      
      // Validar antes de ejecutar
      if (!sonEntradasValidas(tol, iter)) return; 
      
      res = Module.calcular_newton_cpp(f, x0, tol, iter);
    } 
    else if (metodo === 'biseccion') {
      const f = document.getElementById('fn_biseccion').value;
      const a = parseFloat(document.getElementById('a_bis').value);
      const b = parseFloat(document.getElementById('b_bis').value);
      const tol = parseFloat(document.getElementById('tol_bis').value);
      const iter = parseInt(document.getElementById('iter_bis').value);
      
      // Validar antes de ejecutar
      if (!sonEntradasValidas(tol, iter)) return; 
      
      res = Module.calcular_biseccion_cpp(f, a, b, tol, iter);
    }
    else if (metodo === 'secante') {
      const f = document.getElementById('fn_secante').value;
      const x0 = parseFloat(document.getElementById('x0_sec').value);
      const x1 = parseFloat(document.getElementById('x1_sec').value);
      const tol = parseFloat(document.getElementById('tol_sec').value);
      const iter = parseInt(document.getElementById('iter_sec').value);
      
      // Validar antes de ejecutar
      if (!sonEntradasValidas(tol, iter)) return; 
      
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
  
  let thead = '';
  let tbody = '';

  // Configuración de columnas por método (Unidad 2)
  if (metodoActivo === 'biseccion') {
    thead = `<tr><th>n</th><th>a</th><th>b</th><th>c</th><th>f(a)</th><th>f(b)</th><th>f(c)</th><th>Error</th></tr>`;
    for(let i=0; i<res.tabla.size(); i++) {
      const fila = res.tabla.get(i);
      tbody += `<tr>
                  <td>${fila.k !== undefined ? fila.k : i+1}</td>
                  <td>${fila.a !== undefined ? fila.a.toFixed(6) : '-'}</td>
                  <td>${fila.b !== undefined ? fila.b.toFixed(6) : '-'}</td>
                  <td>${(fila.c !== undefined ? fila.c : fila.xr).toFixed(6)}</td>
                  <td>${fila.fa !== undefined ? fila.fa.toFixed(6) : '-'}</td>
                  <td>${fila.fb !== undefined ? fila.fb.toFixed(6) : '-'}</td>
                  <td>${fila.fc !== undefined ? fila.fc.toFixed(6) : '-'}</td>
                  <td>${fila.error.toExponential(4)}</td>
                </tr>`;
    }
  } 
  else if (metodoActivo === 'newton') {
    thead = `<tr><th>n</th><th>Xn</th><th>f(Xn)</th><th>f'(Xn)</th><th>Error</th></tr>`;
    for(let i=0; i<res.tabla.size(); i++) {
      const fila = res.tabla.get(i);
      tbody += `<tr>
                  <td>${fila.k !== undefined ? fila.k : i}</td>
                  <td>${(fila.xk !== undefined ? fila.xk : fila.xr).toFixed(6)}</td>
                  <td>${fila.fx !== undefined ? fila.fx.toFixed(6) : '-'}</td>
                  <td>${fila.dfx !== undefined ? fila.dfx.toFixed(6) : '-'}</td>
                  <td>${fila.error.toExponential(4)}</td>
                </tr>`;
    }
  } 
  else if (metodoActivo === 'secante') {
    thead = `<tr><th>n</th><th>Xn-1</th><th>Xn</th><th>f(Xn-1)</th><th>f(Xn)</th><th>Error</th></tr>`;
    for(let i=0; i<res.tabla.size(); i++) {
      const fila = res.tabla.get(i);
      tbody += `<tr>
                  <td>${fila.k !== undefined ? fila.k : i+1}</td>
                  <td>${fila.x_prev !== undefined ? fila.x_prev.toFixed(6) : '-'}</td>
                  <td>${(fila.xk !== undefined ? fila.xk : fila.xr).toFixed(6)}</td>
                  <td>${fila.fx_prev !== undefined ? fila.fx_prev.toFixed(6) : '-'}</td>
                  <td>${fila.fx !== undefined ? fila.fx.toFixed(6) : '-'}</td>
                  <td>${fila.error.toExponential(4)}</td>
                </tr>`;
    }
  }

  // Insertar los encabezados y el cuerpo generados
  const tablaContenedor = document.querySelector('#resU2 .table-container table');
  if(tablaContenedor) {
    tablaContenedor.innerHTML = `<thead>${thead}</thead><tbody id="tablaU2">${tbody}</tbody>`;
  }
}

// ==========================================
// 4. EJECUTORES MATRICIALES (U3, U4, U5)
// ==========================================

function ejecutarMatricial() {
  if (typeof Module === 'undefined') return alert("Cargando módulo...");

  const n = parseInt(document.getElementById('dimN').value);

  // Validar antes de mandar a C++
  if (isNaN(n) || n < 2 || n > 10) {
      alert("Error: La dimensión de la matriz debe estar entre 2 y 10.");
      return; // Detiene la ejecución
  }
  
  const tol = parseFloat(document.getElementById('tolMat').value);

  // Validar la tolerancia (Aplica para Jacobi, Gauss-Seidel y Potencia)
  if (isNaN(tol) || tol < 0.00000001 || tol > 0.1) {
      alert("Error: La tolerancia debe ser un valor positivo entre 0.00000001 y 0.1");
      return;
  }
  
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

  // ==========================================
  // TABLAS ITERATIVAS (Jacobi y Gauss-Seidel)
  // ==========================================
  if (res.tabla && (metodoActivo === 'jacobi' || metodoActivo === 'gaussseidel')) {
    let nVars = parseInt(document.getElementById('dimN').value); // Tomamos la dimensión del input
    
    // Generar SIEMPRE X1, X2, X3... sin importar el tamaño del sistema
    let varNames = Array.from({length: nVars}, (_, k) => `X${k+1}`);
    
    let numIters = res.tabla.size();
    
    // Obtener la última iteración para leer los datos finales
    let lastRow = numIters > 0 ? res.tabla.get(numIters - 1) : null;
    let totalIters = lastRow && lastRow.k !== undefined ? lastRow.k : Math.max(0, numIters - 1);

    // Función segura para leer el valor final (intenta de res.x, si no, lo saca de la tabla)
    const getFinalVal = (index) => {
        if (res.x) return res.x.get(index);
        if (lastRow) return lastRow.valores !== undefined ? lastRow.valores.get(index) : lastRow.get(index);
        return 0;
    };

    // 1. Tarjetas Superiores (Valores Obtenidos e Iteraciones)
    html += `
    <div class="results-cards-container" style="display: flex; gap: 20px; margin-bottom: 20px;">
        <div class="result-card" style="flex: 1; padding: 15px; border: 1px solid #e0e0e0; border-radius: 8px; background: #f8f9fa;">
            <h4 style="margin: 0 0 10px 0; color: #333; font-size: 14px;">Valores Obtenidos</h4>
            <div style="font-size: 16px; font-weight: bold; color: #0d6efd; display: flex; flex-wrap: wrap; gap: 15px;">`;
    
    for(let i = 0; i < nVars; i++) {
        html += `<span>${varNames[i]} = ${getFinalVal(i).toFixed(6)}</span>`;
    }
    
    html += `
            </div>
        </div>
        <div class="result-card" style="flex: 1; padding: 15px; border: 1px solid #e0e0e0; border-radius: 8px; background: #f8f9fa;">
            <h4 style="margin: 0 0 10px 0; color: #333; font-size: 14px;">Iteraciones</h4>
            <div style="font-size: 18px; font-weight: bold; color: #0d6efd;">${totalIters}</div>
        </div>
    </div>`;

    // 2. Tabla Iterativa Separada
    html += `<div class="table-container" style="border: 1px solid #e0e0e0; border-radius: 8px; overflow: auto; margin-bottom: 20px;">
               <table class="results-table" style="width: 100%; border-collapse: collapse; text-align: left;">
                 <thead style="background-color: #f8f9fa; border-bottom: 1px solid #e0e0e0;">
                   <tr>
                     <th style="padding: 12px;">N</th>
                     ${varNames.map(v => `<th style="padding: 12px;">${v}</th>`).join('')}
                     <th style="padding: 12px;">Error</th>
                   </tr>
                 </thead>
                 <tbody>`;
    
    for (let i = 0; i < numIters; i++) {
      let iterRow = res.tabla.get(i);
      let numK = iterRow.k !== undefined ? iterRow.k : i;
      html += `<tr style="border-bottom: 1px solid #f0f0f0;">
                 <td style="padding: 12px;">${numK}</td>`;
      
      for (let j = 0; j < nVars; j++) {
        let val = iterRow.valores !== undefined ? iterRow.valores.get(j) : iterRow.get(j);
        html += `<td style="padding: 12px;">${val.toFixed(4)}</td>`;
      }
      
      let errorVal = (i === 0) ? "0.0000e+0" : iterRow.error.toExponential(4);
      html += `<td style="padding: 12px;">${errorVal}</td></tr>`;
    }
    html += `</tbody></table></div>`;

    // 3. Vector Solución al fondo
    html += `
    <div class="vector-bottom" style="margin-bottom: 20px;">
        <h4 style="margin-bottom: 10px;">Vector Solución</h4>
        <div style="font-family: monospace; font-size: 16px; background: #f8f9fa; padding: 10px; border-radius: 5px; display: inline-block;">
            [ `;
    for(let i = 0; i < nVars; i++) {
        html += `<span style="margin: 0 10px;">${getFinalVal(i).toFixed(6)}</span>`;
    }
    html += ` ]
        </div>
    </div>`;
  }
  
  // ==========================================
  // MOSTRAR MATRICES L Y U (Para Unidad 4)
  // ==========================================
  if (res.L && res.U) {
    html += `<div style="display: flex; gap: 40px; justify-content: center; margin-bottom: 20px; flex-wrap: wrap;">`;
    
    html += `<div><h4>Matriz L</h4><div class="matrix-bracket-small" style="text-align: center; padding: 10px;">`;
    for (let i = 0; i < res.L.size(); i++) {
      let row = res.L.get(i);
      html += `<div>`;
      for (let j = 0; j < row.size(); j++) {
        html += `<span class="vector-val" style="display:inline-block; width:65px; text-align:right; margin: 0 5px;">${row.get(j).toFixed(4)}</span>`;
      }
      html += `</div>`;
    }
    html += `</div></div>`;

    html += `<div><h4>Matriz U</h4><div class="matrix-bracket-small" style="text-align: center; padding: 10px;">`;
    for (let i = 0; i < res.U.size(); i++) {
      let row = res.U.get(i);
      html += `<div>`;
      for (let j = 0; j < row.size(); j++) {
        html += `<span class="vector-val" style="display:inline-block; width:65px; text-align:right; margin: 0 5px;">${row.get(j).toFixed(4)}</span>`;
      }
      html += `</div>`;
    }
    html += `</div></div>`;
    
    html += `</div>`;
  }

  // ==========================================
  // MOSTRAR VECTORES FINALES
  // ==========================================
  let vecSol = res.x || res.x_solucion || res.v_propio;
  if(vecSol) {
    let tituloVector = (unidadActiva === 'unidad5') ? "Vector Propio" : "Vector Solución";
    html += `<h4>${tituloVector}</h4><div class="matrix-bracket-small" style="text-align: center;">[ `;
    for(let i=0; i<vecSol.size(); i++) html += `<span class="vector-val" style="margin: 0 5px;">${vecSol.get(i).toFixed(6)}</span> `;
    html += `]</div>`;
  }

  if(res.lambda_max !== undefined) html += `<h4 style="margin-top: 15px;">Eigenvalor dominante:</h4> <p>${res.lambda_max.toFixed(6)}</p>`;
  if(res.lambda_min !== undefined) html += `<h4 style="margin-top: 15px;">Eigenvalor mínimo:</h4> <p>${res.lambda_min.toFixed(6)}</p>`;

  sal.innerHTML = html;
}

// ==========================================
// 5. TECLADO MATEMÁTICO (U2)
// ==========================================
function insertarSimbolo(simbolo) {
  let inputId = '';
  
  // Identificar qué input está activo actualmente
  if (metodoActivo === 'biseccion') inputId = 'fn_biseccion';
  else if (metodoActivo === 'newton') inputId = 'fn_newton';
  else if (metodoActivo === 'secante') inputId = 'fn_secante';

  if (inputId) {
    const input = document.getElementById(inputId);
    const inicio = input.selectionStart;
    const fin = input.selectionEnd;
    const texto = input.value;
    
    // Insertar el símbolo donde esté el cursor
    input.value = texto.slice(0, inicio) + simbolo + texto.slice(fin);
    
    // Regresar el foco al input y mover el cursor después del símbolo
    input.focus();
    input.selectionStart = input.selectionEnd = inicio + simbolo.length;
  }
}

function sonEntradasValidas(tolerancia, iteraciones) {
    // Validar Tolerancia
    if (tolerancia < 0.00000001 || tolerancia > 0.1) {
        alert("Error: La tolerancia debe ser un valor positivo entre 0.00000001 y 0.1");
        return false;
    }
    
    // Validar Iteraciones
    if (iteraciones < 1 || iteraciones > 1000 || !Number.isInteger(iteraciones)) {
        alert("Error: Las iteraciones deben ser un número entero entre 1 y 1000.");
        return false;
    }
    
    return true;
}
