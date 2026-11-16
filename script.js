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
function mostrarSeccion(idUnidad) {
    // 1. Ocultar todas las unidades principales
    document.getElementById('unidad2').classList.add('seccion-oculta');
    document.getElementById('unidad5').classList.add('seccion-oculta');
    
    // (Si tienes los divs de unidad3 y unidad4, agrégalos aquí también)
    // document.getElementById('unidad3').classList.add('seccion-oculta');
    // document.getElementById('unidad4').classList.add('seccion-oculta');

    // 2. Mostrar solo la unidad que se clickeó
    document.getElementById(idUnidad).classList.remove('seccion-oculta');
}

function seleccionarMetodo(metodo) {
  console.log("Método seleccionado:", metodo);
}

function confirmarSalida() {
  if (confirm("¿Estás seguro de que deseas salir de Numérica?")) {
    window.close();
  }
}

// 4. Ejecución del Método de Newton-Raphson 100% con WebAssembly (C++ / Embind)
function ejecutarNewton() {
  // Verificar que el módulo C++ esté cargado
  if (typeof Module === 'undefined' || typeof Module.calcular_newton_cpp !== 'function') {
    alert("Cargando módulo de C++...");
    return;
  }

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

  let res = null;

  try {
    // Llamada directa al código compilado C++
    res = Module.calcular_newton_cpp(funcionStr, x0, tol, maxIter);

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

    // Poblar la tabla desde el std::vector<FilaIteracion> (VectorFilas)
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
        </tr>
      `;
    }

    cuerpoTabla.innerHTML = htmlFilas;

  } catch (err) {
    alertaError.innerText = "Error inesperado al ejecutar el algoritmo: " + (err && err.message ? err.message : err);
    alertaError.style.display = 'block';
  } finally {
    // Liberar la memoria del vector de C++
    if (res && res.tabla && typeof res.tabla.delete === 'function') {
      res.tabla.delete();
    }
  }
}
