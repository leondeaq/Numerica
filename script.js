/**
 * NUMÉRICA - Calculadora de Métodos Numéricos
 * JavaScript principal para manejo de la interfaz y puente con WebAssembly (Emscripten / C++)
 */

// ==========================================
// UTILIDADES GLOBALES DE INTERFAZ
// ==========================================

function seleccionarMetodo(metodo) {
  // Cambiar visualización de vistas
  document.getElementById('vista-newton').classList.toggle('hidden', metodo !== 'newton');
  document.getElementById('vista-potencia').classList.toggle('hidden', metodo !== 'potencia');
  
  // Cambiar clases activas en el sidebar
  document.getElementById('nav-newton').classList.toggle('active', metodo === 'newton');
  document.getElementById('nav-potencia').classList.toggle('active', metodo === 'potencia');
}

function mostrarSeccion(seccion) {
  alert(`Seleccionaste: ${seccion}. Cambia desde el menú lateral a los métodos correspondientes.`);
}

function confirmarSalida() {
  if (confirm("¿Estás seguro de que deseas salir de Numérica?")) {
    window.close();
  }
}

// ==========================================
// LÓGICA: MÉTODO DE NEWTON-RAPHSON
// ==========================================

function insertarSimbolo(simbolo) {
  const input = document.getElementById('inputFuncion');
  const start = input.selectionStart;
  const end = input.selectionEnd;
  const textoActual = input.value;
  input.value = textoActual.substring(0, start) + simbolo + textoActual.substring(end);
  input.focus();
  input.setSelectionRange(start + simbolo.length, start + simbolo.length);
}

function cargarEjemploNewton() {
  document.getElementById('inputFuncion').value = 'x^3 - 2*x - 5';
  document.getElementById('inputX0').value = '2.0';
  document.getElementById('inputTol').value = '0.0001';
  document.getElementById('inputMaxIter').value = '100';
}

function ejecutarNewton() {
  if (typeof Module === 'undefined' || typeof Module.calcular_newton_cpp !== 'function') {
    alert("Cargando módulo de C++... Intente de nuevo en unos segundos.");
    return;
  }

  const funcionStr = document.getElementById('inputFuncion').value.trim();
  const x0 = parseFloat(document.getElementById('inputX0').value);
  const tol = parseFloat(document.getElementById('inputTol').value);
  const maxIter = parseInt(document.getElementById('inputMaxIter').value);

  const cardResultados = document.getElementById('cardResultadosNewton');
  const alertaError = document.getElementById('alertaErrorNewton');
  const cuerpoTabla = document.getElementById('cuerpoTablaNewton');

  alertaError.classList.add('hidden');
  cuerpoTabla.innerHTML = '';
  cardResultados.classList.remove('hidden');

  let res = null;

  try {
    res = Module.calcular_newton_cpp(funcionStr, x0, tol, maxIter);

    if (!res.exito && (!res.tabla || res.tabla.size() === 0)) {
      alertaError.innerText = res.error_msg || "Error al procesar el método.";
      alertaError.classList.remove('hidden');
      return;
    }

    document.getElementById('valRaizNewton').innerText = Number(res.raiz).toFixed(6);
    document.getElementById('valIteracionesNewton').innerText = res.iteraciones;

    let htmlFilas = '';
    const totalFilas = res.tabla.size();

    for (let i = 0; i < totalFilas; i++) {
      const fila = res.tabla.get(i);
      htmlFilas += `
        <tr>
          <td>${fila.k}</td>
          <td>${Number(fila.xk).toFixed(6)}</td>
          <td>${Number(fila.f_xk).toExponential(4)}</td>
          <td>${Number(fila.df_xk).toFixed(4)}</td>
          <td>${Number(fila.error).toExponential(4)}</td>
        </tr>`;
    }
    cuerpoTabla.innerHTML = htmlFilas;

    if (!res.exito) {
        alertaError.innerText = res.error_msg;
        alertaError.classList.remove('hidden');
    }

  } catch (err) {
    alertaError.innerText = "Error inesperado al ejecutar el algoritmo: " + err;
    alertaError.classList.remove('hidden');
  } finally {
    if (res && res.tabla && typeof res.tabla.delete === 'function') {
      res.tabla.delete();
    }
  }
}

// ==========================================
// LÓGICA: MÉTODO DE LA POTENCIA
// ==========================================

function generarGrillaPotencia() {
  const n = parseInt(document.getElementById('inputN').value);
  if (isNaN(n) || n < 2 || n > 10) return;

  const containerA = document.getElementById('matriz-A-container');
  const containerX = document.getElementById('vector-x-container');
  
  // Establecer columnas CSS Grid para la matriz
  containerA.style.gridTemplateColumns = `repeat(${n}, 1fr)`;
  containerA.innerHTML = '';
  
  // Generar inputs para la Matriz A
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      const input = document.createElement('input');
      input.type = 'number';
      input.step = 'any';
      input.className = 'matrix-input';
      input.id = `A_${i}_${j}`;
      input.value = (i === j) ? '1' : '0'; // Identidad por defecto
      input.required = true;
      containerA.appendChild(input);
    }
  }

  // Generar inputs para el Vector x0
  containerX.style.gridTemplateColumns = `1fr`;
  containerX.innerHTML = '';
  for (let i = 0; i < n; i++) {
    const input = document.createElement('input');
    input.type = 'number';
    input.step = 'any';
    input.className = 'matrix-input vector-input';
    input.id = `x_${i}`;
    input.value = '1'; // Vector de unos por defecto
    input.required = true;
    containerX.appendChild(input);
  }
}

function cargarEjemploPotencia() {
  document.getElementById('inputN').value = 3;
  document.getElementById('inputTolPot').value = 0.001;
  document.getElementById('inputMaxIterPot').value = 50;
  generarGrillaPotencia();
  
  // Matriz de ejemplo clásica que converge rápido
  const valoresA = [
    [4, 1, 0],
    [0, 2, 1],
    [0, 0, -1]
  ];
  const valoresX = [1, 1, 1];

  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      document.getElementById(`A_${i}_${j}`).value = valoresA[i][j];
    }
    document.getElementById(`x_${i}`).value = valoresX[i];
  }
}

function ejecutarPotencia() {
  if (typeof Module === 'undefined' || typeof Module.calcular_potencia_cpp !== 'function') {
    alert("Cargando módulo de C++... Intente de nuevo en unos segundos.");
    return;
  }

  const n = parseInt(document.getElementById('inputN').value);
  const tol = parseFloat(document.getElementById('inputTolPot').value);
  const maxIter = parseInt(document.getElementById('inputMaxIterPot').value);

  const cardResultados = document.getElementById('cardResultadosPotencia');
  const alertaError = document.getElementById('alertaErrorPotencia');
  const cuerpoTabla = document.getElementById('cuerpoTablaPotencia');

  alertaError.classList.add('hidden');
  cuerpoTabla.innerHTML = '';
  cardResultados.classList.remove('hidden');

  let vecMatriz = null;
  let vecX = null;
  let res = null;

  try {
    // 1. Convertir datos de JS (DOM) a std::vector de Embind (C++)
    vecMatriz = new Module.VectorMatriz();
    for (let i = 0; i < n; i++) {
      let filaC = new Module.VectorDouble();
      for (let j = 0; j < n; j++) {
        let val = parseFloat(document.getElementById(`A_${i}_${j}`).value);
        filaC.push_back(val);
      }
      vecMatriz.push_back(filaC);
      filaC.delete(); // Prevenir memory leak del vector interno
    }

    vecX = new Module.VectorDouble();
    for (let i = 0; i < n; i++) {
      let val = parseFloat(document.getElementById(`x_${i}`).value);
      vecX.push_back(val);
    }

    // 2. Ejecutar función C++
    res = Module.calcular_potencia_cpp(n, vecMatriz, vecX, tol, maxIter);

    // 3. Procesar resultados
    if (!res.exito && res.tabla.size() === 0) {
      alertaError.innerText = res.error_msg || "Error matemático en el método de la potencia.";
      alertaError.classList.remove('hidden');
      return;
    }

    // Mostrar métricas y Eigenvector
    document.getElementById('valEigenvalor').innerText = Number(res.eigenvalor).toFixed(6);
    document.getElementById('valIteracionesPotencia').innerText = res.iteraciones;

    let htmlEigenvector = '';
    const eigenSize = res.eigenvector.size();
    for(let i=0; i<eigenSize; i++){
       htmlEigenvector += `<div class="vector-val">${Number(res.eigenvector.get(i)).toFixed(5)}</div>`;
    }
    document.getElementById('valEigenvector').innerHTML = htmlEigenvector;

    // Llenar tabla de iteraciones
    let htmlFilas = '';
    const totalFilas = res.tabla.size();
    for (let i = 0; i < totalFilas; i++) {
      const fila = res.tabla.get(i);
      htmlFilas += `
        <tr>
          <td>${fila.k}</td>
          <td>${Number(fila.eigenvalor).toFixed(6)}</td>
          <td>${fila.k === 1 ? '---' : Number(fila.error).toExponential(4)}</td>
        </tr>`;
    }
    cuerpoTabla.innerHTML = htmlFilas;

    if (!res.exito) {
        alertaError.innerText = res.error_msg;
        alertaError.classList.remove('hidden');
    }

  } catch (err) {
    alertaError.innerText = "Error inesperado al ejecutar el algoritmo: " + err;
    alertaError.classList.remove('hidden');
  } finally {
    // 4. Liberar la memoria explícitamente (Regla estricta de Emscripten)
    if (vecMatriz) vecMatriz.delete();
    if (vecX) vecX.delete();
    if (res) {
      if (res.tabla) res.tabla.delete();
      if (res.eigenvector) res.eigenvector.delete();
    }
  }
}
