/**
 * NUMÉRICA - Calculadora de Métodos Numéricos
 * JavaScript principal para manejo de la interfaz y puente con WebAssembly (Emscripten / C++)
 */

// 1. Insertar símbolos en la posición actual del cursor en f(x)
function insertarSimbolo(simbolo) {
  const input = document.getElementById('inputFuncion');
  const start = input.selectionStart;
  const end = input.selectionEnd;
  const textoActual = input.value;

  input.value = textoActual.substring(0, start) + simbolo + textoActual.substring(end);
  input.focus();
  input.setSelectionRange(start + simbolo.length, start + simbolo.length);
}

// 2. Cargar datos del ejemplo por defecto
function cargarEjemploNewton() {
  document.getElementById('inputFuncion').value = 'x^3 - 2*x - 5';
  document.getElementById('inputX0').value = '2.0';
  document.getElementById('inputTol').value = '0.0001';
  document.getElementById('inputMaxIter').value = '100';
}

// 3. Navegación entre vistas/secciones
function mostrarSeccion(seccion) {
  alert(`Seleccionaste: ${seccion}. La Unidad 2 es la activa en esta demostración.`);
}

function seleccionarMetodo(metodo) {
  console.log("Método seleccionado:", metodo);
}

function confirmarSalida() {
  if (confirm("¿Estás seguro de que deseas salir de Numérica?")) {
    window.close();
  }
}

// 4. Ejecución del Método de Newton-Raphson mediante WebAssembly (C++)
function ejecutarNewton() {
  const funcionStr = document.getElementById('inputFuncion').value.trim();
  const x0 = parseFloat(document.getElementById('inputX0').value);
  const tol = parseFloat(document.getElementById('inputTol').value);
  const maxIter = parseInt(document.getElementById('inputMaxIter').value);

  const cardResultados = document.getElementById('cardResultados');
  const alertaError = document.getElementById('alertaError');
  const cuerpoTabla = document.getElementById('cuerpoTabla');

  // Limpiar estados previos
  alertaError.style.display = 'none';
  cuerpoTabla.innerHTML = '';
  cardResultados.style.display = 'block';

  try {
    let res;

    // Verificar si el módulo C++ (Emscripten) está disponible globalmente
    if (typeof Module !== 'undefined' && typeof Module.calcular_newton_cpp === 'function') {
      // Llamada directa al código compilado C++
      res = Module.calcular_newton_cpp(funcionStr, x0, tol, maxIter);
    } else {
      // Fallback en JavaScript en caso de que aún no se haya cargado core.js/core.wasm
      console.warn("Módulo WebAssembly C++ cargando o no detectado. Utilizando motor fallback.");
      res = ejecutarNewtonFallbackJS(funcionStr, x0, tol, maxIter);
    }

    if (!res.exito) {
      alertaError.innerText = res.error_msg || "Error al procesar el método.";
      alertaError.style.display = 'block';
      document.getElementById('valRaiz').innerText = "-";
      document.getElementById('valIteraciones').innerText = "-";
      return;
    }

    // Actualizar métricas
    document.getElementById('valRaiz').innerText = Number(res.raiz).toFixed(6);
    document.getElementById('valIteraciones').innerText = res.iteraciones;

    // Poblar la tabla de iteraciones
    let htmlFilas = '';
    
    // Obtener array de iteraciones (funciona tanto para std::vector embebido en Embind como para arrays en JS)
    const totalFilas = typeof res.tabla.size === 'function' ? res.tabla.size() : res.tabla.length;

    for (let i = 0; i < totalFilas; i++) {
      const fila = typeof res.tabla.get === 'function' ? res.tabla.get(i) : res.tabla[i];
      
      htmlFilas += `
        <tr>
          <td>${fila.k}</td>
          <td>${Number(fila.xk).toFixed(6)}</td>
          <td>${Number(fila.f_xk).toExponential(4)}</td>
          <td>${Number(fila.df_xk).toFixed(4)}</td>
          <td>${Number(fila.error).toExponential(4)}</td>
        </tr>
      `;
    }

    cuerpoTabla.innerHTML = htmlFilas;

  } catch (err) {
    alertaError.innerText = "Error inesperado al ejecutar el algoritmo: " + err.message;
    alertaError.style.display = 'block';
  }
}

/**
 * Motor Fallback en JS para evaluación previa mientras se compila el ejecutable C++.
 */
function ejecutarNewtonFallbackJS(exprStr, x0, tol, maxIter) {
  // Evaluador básico de funciones comunes
  function f(x) {
    // Reemplazos simples de notación matemática a Math.js
    let parsed = exprStr
      .replace(/x/g, `(${x})`)
      .replace(/\^/g, '**')
      .replace(/sin/g, 'Math.sin')
      .replace(/cos/g, 'Math.cos')
      .replace(/sqrt/g, 'Math.sqrt')
      .replace(/exp/g, 'Math.exp');
    return Function(`"use strict"; return (${parsed})`)();
  }

  // Diferencia finita centrada para derivada
  const h = 1e-7;
  function df(x) {
    return (f(x + h) - f(x - h)) / (2 * h);
  }

  let xk = x0;
  let tabla = [];

  for (let k = 1; k <= maxIter; k++) {
    let f_xk = f(xk);
    let df_xk = df(xk);

    if (Math.abs(df_xk) < 1e-12) {
      return { exito: false, error_msg: "La derivada f'(x) es cercana a cero." };
    }

    let x_next = xk - (f_xk / df_xk);
    let error = Math.abs(x_next) > 1e-12 ? Math.abs((x_next - xk) / x_next) : Math.abs(x_next - xk);

    tabla.push({ k: k, xk: xk, f_xk: f_xk, df_xk: df_xk, error: error });

    if (error < tol) {
      return { exito: true, raiz: x_next, iteraciones: k, tabla: tabla };
    }

    xk = x_next;
  }

  return { exito: true, raiz: xk, iteraciones: maxIter, tabla: tabla };
}
