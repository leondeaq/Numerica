// ============================================================================
// core.cpp - Proyecto "Numérica"
// Los 10 métodos numéricos en un solo módulo WebAssembly (Emscripten + Embind)
//
// Compilación sugerida:
//   emcc core.cpp -o core.js -lembind -O2 -s MODULARIZE=1 -s EXPORT_NAME="createModule" -s ALLOW_MEMORY_GROWTH=1
//   Nota: Solo si se decide compilar localmente en pc.
// Contenido:
//   UNIDAD 2: Bisección, Newton-Raphson, Secante
//   UNIDAD 3: Gauss-Jordan, Doolittle (LU), Crout (LU)
//   UNIDAD 4: Jacobi, Gauss-Seidel
//   UNIDAD 5: Potencia, Potencia Inversa
// ============================================================================

#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <emscripten/bind.h>

using namespace std;
using namespace emscripten;

// ============================================================================
// UTILIDADES COMUNES
// ============================================================================

// Valida que A sea n x n. Devuelve "" si está bien, o el motivo exacto del error.
string validar_matriz_cuadrada(const vector<vector<double>>& A) {
    int n = A.size();

    if (n == 0) {
        return "La matriz A está vacía.";
    }

    for (int i = 0; i < n; i++) {
        if ((int)A[i].size() != n) {
            return "La matriz A debe ser cuadrada (n x n): la fila " + to_string(i + 1) +
                   " tiene " + to_string(A[i].size()) + " columnas y se esperaban " +
                   to_string(n) + ".";
        }
    }

    return "";
}

// Valida que un vector tenga exactamente n elementos.
string validar_vector(const vector<double>& v, int n, const string& nombre) {
    if ((int)v.size() != n) {
        return "El vector " + nombre + " debe tener " + to_string(n) +
               " elementos (misma dimensión que A) y tiene " + to_string(v.size()) + ".";
    }

    return "";
}

// Valida A (cuadrada) y el vector b (misma dimensión).
string validar_sistema(const vector<vector<double>>& A, const vector<double>& b) {
    string msg = validar_matriz_cuadrada(A);
    if (!msg.empty()) return msg;

    return validar_vector(b, A.size(), "b");
}

// Indica si A es estrictamente diagonal dominante por filas.
bool es_diagonal_dominante(const vector<vector<double>>& A) {
    int n = A.size();

    for (int i = 0; i < n; i++) {
        double suma = 0;

        for (int j = 0; j < n; j++) {
            if (i != j) suma += fabs(A[i][j]);
        }

        if (fabs(A[i][i]) <= suma) return false;
    }

    return true;
}

// Devuelve el número de la primera fila con ceros en la diagonal, o 0 si no hay.
int fila_con_diagonal_cero(const vector<vector<double>>& A) {
    int n = A.size();

    for (int i = 0; i < n; i++) {
        if (fabs(A[i][i]) < 1e-12) return i + 1;
    }

    return 0;
}

// ============================================================================
// EVALUADOR MATEMÁTICO (parser de f(x) escrita por el usuario)
// Soporta: + - * / ^ , paréntesis, x, pi, e,
//          sin cos tan sqrt exp ln log(base 10) log10 abs
// ============================================================================
class EvaluadorMatematico {
private:
    string expr;
    size_t pos;
    double val_x;

    // Mira el siguiente carácter (ignorando espacios) sin consumirlo
    char peek() {
        while (pos < expr.length() && isspace((unsigned char)expr[pos])) {
            pos++;
        }

        if (pos >= expr.length()) return '\0';

        return expr[pos];
    }

    // Consume el siguiente carácter
    char get() {
        char c = peek();

        if (c != '\0') pos++;

        return c;
    }

    // Lee un número: 3, 3.14, .5, 1e-5
    double parse_numero() {
        size_t inicio = pos;

        while (pos < expr.length() &&
               (isdigit((unsigned char)expr[pos]) || expr[pos] == '.')) {
            pos++;
        }

        // Notación científica opcional (e o E seguida de dígitos)
        if (pos < expr.length() && (expr[pos] == 'e' || expr[pos] == 'E')) {
            size_t p = pos + 1;

            if (p < expr.length() && (expr[p] == '+' || expr[p] == '-')) p++;

            if (p < expr.length() && isdigit((unsigned char)expr[p])) {
                pos = p;

                while (pos < expr.length() && isdigit((unsigned char)expr[pos])) {
                    pos++;
                }
            }
        }

        string texto = expr.substr(inicio, pos - inicio);
        size_t usados = 0;
        double valor = 0;

        try {
            valor = stod(texto, &usados);
        } catch (...) {
            throw runtime_error("Número mal escrito: " + texto);
        }

        if (usados != texto.length()) {
            throw runtime_error("Número mal escrito: " + texto);
        }

        return valor;
    }

    // Número, constante, variable, función o expresión entre paréntesis
    double parse_primario() {
        char c = peek();

        if (c == '(') {
            get();

            double valor = parse_expresion();

            if (get() != ')') {
                throw runtime_error("Falta cerrar paréntesis.");
            }

            return valor;
        }

        if (isdigit((unsigned char)c) || c == '.') {
            return parse_numero();
        }

        if (isalpha((unsigned char)c)) {
            size_t inicio = pos;

            while (pos < expr.length() && isalnum((unsigned char)expr[pos])) {
                pos++;
            }

            string nombre = expr.substr(inicio, pos - inicio);

            // Todo a minúsculas para aceptar SIN, Sin, sin, X, PI...
            for (size_t i = 0; i < nombre.length(); i++) {
                nombre[i] = tolower((unsigned char)nombre[i]);
            }

            if (nombre == "x") return val_x;
            if (nombre == "pi") return 3.14159265358979323846;
            if (nombre == "e") return 2.71828182845904523536;

            if (peek() == '(') {
                get();

                double arg = parse_expresion();

                if (get() != ')') {
                    throw runtime_error("Falta cerrar paréntesis.");
                }

                if (nombre == "sin") return sin(arg);
                if (nombre == "cos") return cos(arg);
                if (nombre == "tan") return tan(arg);
                if (nombre == "exp") return exp(arg);
                if (nombre == "abs") return fabs(arg);

                if (nombre == "sqrt") {
                    if (arg < 0) throw runtime_error("sqrt de un número negativo.");
                    return sqrt(arg);
                }

                if (nombre == "ln") {
                    if (arg <= 0) throw runtime_error("ln de un número no positivo.");
                    return log(arg);
                }

                if (nombre == "log" || nombre == "log10") {
                    if (arg <= 0) throw runtime_error("log de un número no positivo.");
                    return log10(arg);
                }

                throw runtime_error("Función no soportada: " + nombre);
            }

            throw runtime_error("Variable no identificada: " + nombre);
        }

        throw runtime_error("Sintaxis matemática inválida.");
    }

    // Potencia (asociativa a la derecha): a^b^c = a^(b^c)
    double parse_potencia() {
        double base = parse_primario();

        if (peek() == '^') {
            get();

            double exponente = parse_unario();

            return pow(base, exponente);
        }

        return base;
    }

    // Signos unarios: -x^2 se interpreta como -(x^2)
    double parse_unario() {
        char c = peek();

        if (c == '-') {
            get();
            return -parse_unario();
        }

        if (c == '+') {
            get();
            return parse_unario();
        }

        return parse_potencia();
    }

    // Multiplicación y división
    double parse_termino() {
        double izquierda = parse_unario();

        while (true) {
            char c = peek();

            if (c == '*') {
                get();
                izquierda *= parse_unario();
            }
            else if (c == '/') {
                get();

                double derecha = parse_unario();

                if (fabs(derecha) < 1e-15) {
                    throw runtime_error("División entre cero.");
                }

                izquierda /= derecha;
            }
            else {
                break;
            }
        }

        return izquierda;
    }

    // Suma y resta
    double parse_expresion() {
        double izquierda = parse_termino();

        while (true) {
            char c = peek();

            if (c == '+') {
                get();
                izquierda += parse_termino();
            }
            else if (c == '-') {
                get();
                izquierda -= parse_termino();
            }
            else {
                break;
            }
        }

        return izquierda;
    }

public:
    EvaluadorMatematico(string e) : expr(e), pos(0), val_x(0) {}

    double evaluar(double x) {
        pos = 0;
        val_x = x;

        if (expr.empty()) {
            throw runtime_error("La función está vacía.");
        }

        double resultado = parse_expresion();

        if (peek() != '\0') {
            throw runtime_error("Error de sintaxis en la expresión (símbolo inesperado o paréntesis de más).");
        }

        if (!isfinite(resultado)) {
            throw runtime_error("La función no es un número real válido en x = " + to_string(x) + ".");
        }

        return resultado;
    }
};

// Evalúa f(x) para la expresión dada
double evaluar_f(string expr, double x) {
    EvaluadorMatematico eval(expr);
    return eval.evaluar(x);
}

// ============================================================================
// UNIDAD 2: MÉTODOS PARA RAÍCES DE ECUACIONES (Bisección, Newton, Secante)
// ============================================================================

// ---------------------------- MÉTODO DE BISECCIÓN ---------------------------

struct FilaBiseccion {
    int k;
    double a;
    double b;
    double xr;
    double fa;
    double fb;
    double fc;
    double error; // En la iteración 1 vale 0 (aún no hay aproximación previa)
};

struct ResultadoBiseccion {
    bool exito;
    double raiz;
    int iteraciones;
    double error_final;
    string error_msg;
    vector<FilaBiseccion> tabla;
};

ResultadoBiseccion calcular_biseccion_cpp(string expr_str, double a, double b, double tol, int max_iter) {
    ResultadoBiseccion res;
    res.exito = false;
    res.raiz = 0;
    res.iteraciones = 0;
    res.error_final = 0;

    try {
        // Validar datos
        if (tol <= 0) throw runtime_error("La tolerancia debe ser positiva.");
        if (max_iter <= 0) throw runtime_error("El máximo de iteraciones debe ser positivo.");
        if (a >= b) throw runtime_error("El límite inferior a debe ser menor que el límite superior b.");

        double fa = evaluar_f(expr_str, a);
        double fb = evaluar_f(expr_str, b);

        // Si un extremo ya es raíz, terminamos
        if (fa == 0) {
            res.exito = true;
            res.raiz = a;
            return res;
        }

        if (fb == 0) {
            res.exito = true;
            res.raiz = b;
            return res;
        }

        // Debe existir cambio de signo en [a, b]
        if ((fa > 0 && fb > 0) || (fa < 0 && fb < 0)) {
            ostringstream msg;
            msg << "El intervalo no tiene cambio de signo: f(a) = " << fa << ", f(b) = " << fb << ".";
            throw runtime_error(msg.str());
        }

        double xr = 0;
        double anterior = 0;
        double error = 0;

        for (int i = 1; i <= max_iter; i++) {

            // 1. Punto medio
            xr = (a + b) / 2.0;
            double fx = evaluar_f(expr_str, xr);

            // 2. Error entre aproximaciones sucesivas (desde la iteración 2)
            if (i > 1) error = fabs(xr - anterior);
            else error = 0;

            res.tabla.push_back({i, a, b, xr, fa, fb, fx, error});

            res.raiz = xr;
            res.iteraciones = i;
            res.error_final = error;

            // 3. Condiciones de paro
            if (fabs(fx) < 1e-12) {
                res.exito = true;
                res.error_final = 0;
                return res;
            }

            if (i > 1 && error <= tol) {
                res.exito = true;
                return res;
            }

            // 4. Reducir el intervalo a la mitad donde hay cambio de signo
            if ((fa < 0 && fx > 0) || (fa > 0 && fx < 0)) {
                b = xr;
                fb = fx;
            } else {
                a = xr;
                fa = fx;
            }

            anterior = xr;
        }

        res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";
        return res;
    }
    catch (const exception& e) {
        res.exito = false;
        res.error_msg = e.what();
        return res;
    }
}

// ------------------------- MÉTODO DE NEWTON-RAPHSON -------------------------

struct FilaIteracion {
    int k;
    double xk;
    double fx;
    double dfx;
    double error;
};

struct ResultadoNewton {
    bool exito;
    double raiz;
    int iteraciones;
    string error_msg;
    vector<FilaIteracion> tabla;
};

ResultadoNewton calcular_newton_cpp(string expr_str, double x0, double tol, int max_iter) {
    ResultadoNewton res;
    res.exito = false;
    res.raiz = 0;
    res.iteraciones = 0;

    try {
        if (tol <= 0) throw runtime_error("La tolerancia debe ser positiva.");
        if (max_iter <= 0) throw runtime_error("El máximo de iteraciones debe ser positivo.");

        double xk = x0;
        double h = 1e-7;   // Paso para la derivada numérica centrada

        for (int k = 1; k <= max_iter; k++) {
            double f_xk = evaluar_f(expr_str, xk);
            double df_xk = (evaluar_f(expr_str, xk + h) - evaluar_f(expr_str, xk - h)) / (2.0 * h);

            if (fabs(df_xk) < 1e-12) {
                res.raiz = xk;
                res.error_msg = "La derivada f'(x) es muy cercana a cero en x = " +
                                to_string(xk) + ". El método no puede continuar.";
                return res;
            }

            double x_next = xk - (f_xk / df_xk);

            if (!isfinite(x_next)) {
                throw runtime_error("El método diverge (se obtuvo un valor no finito).");
            }

            // Error relativo (si x es casi 0 se usa el error absoluto)
            double error;
            if (fabs(x_next) > 1e-12) error = fabs((x_next - xk) / x_next);
            else error = fabs(x_next - xk);

            res.tabla.push_back({k, xk, f_xk, df_xk, error});

            res.raiz = x_next;
            res.iteraciones = k;

            if (error < tol) {
                res.exito = true;
                return res;
            }

            xk = x_next;
        }

        res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";
        return res;
    }
    catch (const exception& e) {
        res.exito = false;
        res.error_msg = e.what();
        return res;
    }
}

// ----------------------------- MÉTODO DE LA SECANTE -------------------------

struct FilaIteracionSecante {
    int k;
    double x_prev;
    double xk;
    double fx_prev;
    double fx;
    double error;    // |f(xk)|; en las filas iniciales (k = 0 y 1) vale 0
};

struct ResultadoSecante {
    bool exito;
    double raiz;
    int iteraciones;
    string error_msg;
    vector<FilaIteracionSecante> tabla;
};

ResultadoSecante calcular_secante_cpp(string expr_str, double x0, double x1, double tol, int max_iter) {
    ResultadoSecante res;
    res.exito = false;
    res.raiz = 0;
    res.iteraciones = 0;

    try {
        if (tol <= 0) throw runtime_error("La tolerancia debe ser positiva.");
        if (max_iter <= 0) throw runtime_error("El máximo de iteraciones debe ser positivo.");

        double fx0 = evaluar_f(expr_str, x0);
        double fx1 = evaluar_f(expr_str, x1);

        // Filas con los dos valores iniciales
        // En la fila 0, no hay un x_prev real, repetimos x0 o ponemos 0. Lo ideal:
        res.tabla.push_back({0, x0, x0, fx0, fx0, 0});
        res.tabla.push_back({1, x0, x1, fx0, fx1, 0});

        for (int i = 1; i <= max_iter; i++) {
            double denominador = fx1 - fx0;

            if (fabs(denominador) < 1e-15) {
                res.raiz = x1;
                throw runtime_error("División entre cero: f(x1) - f(x0) es prácticamente cero.");
            }

            // Fórmula de la secante
            double x2 = x1 - (fx1 * (x1 - x0)) / denominador;
            double fx2 = evaluar_f(expr_str, x2);
            double error = fabs(fx2);

            res.tabla.push_back({i + 1, x1, x2, fx1, fx2, error});

            res.raiz = x2;
            res.iteraciones = i;

            if (error < tol) {
                res.exito = true;
                return res;
            }

            // Desplazar los valores para la siguiente iteración
            x0 = x1;
            fx0 = fx1;
            x1 = x2;
            fx1 = fx2;
        }

        res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";
        return res;
    }
    catch (const exception& e) {
        res.exito = false;
        res.error_msg = e.what();
        return res;
    }
}

// ===============================================================================
// UNIDAD 3: SISTEMAS DE ECUACIONES LINEALES (Gauss-Jordan, Jacobi, Gauss-Seidel)
// ===============================================================================

// ------------------------------ GAUSS-JORDAN --------------------------------

// Cada paso guarda una copia de la matriz aumentada [A|b]
struct PasoGaussJordan {
    int paso;
    string descripcion;
    vector<vector<double>> matriz_estado;
};

struct ResultadoGaussJordan {
    bool exito;
    string error_msg;
    vector<double> x;
    vector<PasoGaussJordan> pasos;
};

ResultadoGaussJordan calcular_gauss_jordan_cpp(vector<vector<double>> A, vector<double> b) {
    ResultadoGaussJordan res;
    res.exito = false;

    // Validaciones de dimensiones
    string msg = validar_sistema(A, b);
    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    int n = A.size();

    // Matriz aumentada [A|b] de tamaño n x (n+1)
    vector<vector<double>> aug(n, vector<double>(n + 1, 0.0));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i][j] = A[i][j];
        }
        aug[i][n] = b[i];
    }

    int contador_pasos = 1;
    res.pasos.push_back({contador_pasos++, "Matriz aumentada inicial [A|b]", aug});

    for (int i = 0; i < n; i++) {

        // 1. Pivoteo parcial: buscar el mayor valor absoluto en la columna i
        int fila_max = i;
        double max_val = fabs(aug[i][i]);

        for (int k = i + 1; k < n; k++) {
            if (fabs(aug[k][i]) > max_val) {
                max_val = fabs(aug[k][i]);
                fila_max = k;
            }
        }

        if (max_val < 1e-12) {
            res.error_msg = "El sistema no tiene solución única (matriz singular o mal condicionada): " +
                            string("no hay pivote válido en la columna ") + to_string(i + 1) + ".";
            return res;
        }

        if (fila_max != i) {
            swap(aug[i], aug[fila_max]);

            ostringstream desc;
            desc << "Intercambio de Fila " << (i + 1) << " con Fila " << (fila_max + 1) << " (pivoteo parcial)";
            res.pasos.push_back({contador_pasos++, desc.str(), aug});
        }

        // 2. Normalizar la fila pivote para que el pivote sea 1
        double pivote = aug[i][i];

        if (pivote != 1.0) {
            for (int j = i; j <= n; j++) {
                aug[i][j] /= pivote;
            }

            ostringstream desc;
            desc << "Fila " << (i + 1) << " <- Fila " << (i + 1) << " / " << pivote;
            res.pasos.push_back({contador_pasos++, desc.str(), aug});
        }

        // 3. Hacer ceros en el resto de la columna i (arriba y abajo)
        for (int k = 0; k < n; k++) {
            if (k != i) {
                double factor = aug[k][i];

                if (fabs(factor) > 1e-12) {
                    for (int j = i; j <= n; j++) {
                        aug[k][j] -= factor * aug[i][j];
                    }

                    ostringstream desc;
                    desc << "Fila " << (k + 1) << " <- Fila " << (k + 1)
                         << " - (" << factor << ") * Fila " << (i + 1);
                    res.pasos.push_back({contador_pasos++, desc.str(), aug});
                }
            }
        }
    }

    // La solución queda en la última columna
    res.x.assign(n, 0.0);

    for (int i = 0; i < n; i++) {
        res.x[i] = aug[i][n];
    }

    res.exito = true;
    return res;
}



// Fila de la tabla de iteraciones (compartida por Jacobi y Gauss-Seidel)
struct FilaIterativaSis {
    int k;
    vector<double> valores;
    double error;   
};

// --------------------------------- JACOBI -----------------------------------

struct ResultadoJacobi {
    bool exito;
    int iteraciones;
    vector<double> x_solucion;
    vector<FilaIterativaSis> tabla;
    string error_msg;
};

ResultadoJacobi calcular_jacobi_cpp(
    vector<vector<double>> A,
    vector<double> b,
    vector<double> x0,
    double tolerancia,
    int max_iter
) {
    ResultadoJacobi res;
    res.exito = false;
    res.iteraciones = 0;

    // Validaciones
    string msg = validar_sistema(A, b);
    if (msg.empty()) msg = validar_vector(x0, A.size(), "x0");

    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    if (tolerancia <= 0) {
        res.error_msg = "La tolerancia debe ser positiva.";
        return res;
    }

    if (max_iter <= 0) {
        res.error_msg = "El máximo de iteraciones debe ser positivo.";
        return res;
    }

    int fila_cero = fila_con_diagonal_cero(A);
    if (fila_cero != 0) {
        res.error_msg = "Hay un cero en la diagonal principal (fila " + to_string(fila_cero) + ").";
        return res;
    }

    int n = A.size();
    bool dominante = es_diagonal_dominante(A);

    vector<double> x = x0;
    vector<double> anterior(n, 0.0);
    vector<double> nuevo(n, 0.0);

    for (int k = 1; k <= max_iter; k++) {

        anterior = x;

        // Jacobi usa SOLO valores de la iteración anterior
        for (int i = 0; i < n; i++) {
            double suma = 0;

            for (int j = 0; j < n; j++) {
                if (i != j) suma += A[i][j] * anterior[j];
            }

            nuevo[i] = (b[i] - suma) / A[i][i];
        }

        // Error máximo entre iteraciones
        double error = 0;

        for (int i = 0; i < n; i++) {
            double diferencia = fabs(nuevo[i] - anterior[i]);
            if (diferencia > error) error = diferencia;
        }

        x = nuevo;

        if (!isfinite(error)) {
            res.error_msg = "El método diverge (se obtuvieron valores no finitos).";
            return res;
        }

        FilaIterativaSis fila;
        fila.k = k;
        fila.valores = x;
        fila.error = error;
        res.tabla.push_back(fila);

        res.iteraciones = k;

        if (error < tolerancia) {
            res.exito = true;
            res.x_solucion = x;

            if (!dominante) {
                res.error_msg = "Advertencia: la matriz no es estrictamente diagonal dominante.";
            }

            return res;
        }
    }

    // No se alcanzó la tolerancia
    res.exito = false;
    res.x_solucion = x;
    res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";

    if (!dominante) {
        res.error_msg += " La matriz tampoco es estrictamente diagonal dominante.";
    }

    return res;
}

// ------------------------------- GAUSS-SEIDEL --------------------------------

struct ResultadoGaussSeidel {
    bool exito;
    int iteraciones;
    vector<double> x_solucion;
    vector<FilaIterativaSis> tabla;
    string error_msg;
};

ResultadoGaussSeidel calcular_gauss_seidel_cpp(
    vector<vector<double>> A,
    vector<double> b,
    vector<double> x0,
    double tolerancia,
    int max_iter
) {
    ResultadoGaussSeidel res;
    res.exito = false;
    res.iteraciones = 0;

    // Validaciones
    string msg = validar_sistema(A, b);
    if (msg.empty()) msg = validar_vector(x0, A.size(), "x0");

    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    if (tolerancia <= 0) {
        res.error_msg = "La tolerancia debe ser positiva.";
        return res;
    }

    if (max_iter <= 0) {
        res.error_msg = "El máximo de iteraciones debe ser positivo.";
        return res;
    }

    int fila_cero = fila_con_diagonal_cero(A);
    if (fila_cero != 0) {
        res.error_msg = "Hay un cero en la diagonal principal (fila " + to_string(fila_cero) + ").";
        return res;
    }

    int n = A.size();
    bool dominante = es_diagonal_dominante(A);

    vector<double> x = x0;

    for (int k = 1; k <= max_iter; k++) {

        vector<double> anterior = x;

        // Gauss-Seidel usa los valores recién calculados dentro de la misma iteración
        for (int i = 0; i < n; i++) {
            double suma = 0;

            for (int j = 0; j < n; j++) {
                if (i != j) suma += A[i][j] * x[j];
            }

            x[i] = (b[i] - suma) / A[i][i];
        }

        // Error máximo entre iteraciones
        double error = 0;

        for (int i = 0; i < n; i++) {
            double diferencia = fabs(x[i] - anterior[i]);
            if (diferencia > error) error = diferencia;
        }

        if (!isfinite(error)) {
            res.error_msg = "El método diverge (se obtuvieron valores no finitos).";
            return res;
        }

        FilaIterativaSis fila;
        fila.k = k;
        fila.valores = x;
        fila.error = error;
        res.tabla.push_back(fila);

        res.iteraciones = k;

        if (error < tolerancia) {
            res.exito = true;
            res.x_solucion = x;

            if (!dominante) {
                res.error_msg = "Advertencia: la matriz no es estrictamente diagonal dominante.";
            }

            return res;
        }
    }

    // No se alcanzó la tolerancia
    res.exito = false;
    res.x_solucion = x;
    res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";

    if (!dominante) {
        res.error_msg += " La matriz tampoco es estrictamente diagonal dominante.";
    }

    return res;
}

// ============================================================================
// UNIDAD 4: FACTORIZACIÓN LU (Doolittle, Crout)
// ============================================================================

// --------------------------- FACTORIZACIÓN LU: DOOLITTLE ---------------------
// Diagonal de L = 1

struct ResultadoDoolittle {
    bool exito;
    string error_msg;
    vector<vector<double>> L;
    vector<vector<double>> U;
    vector<double> y;
    vector<double> x;
};

ResultadoDoolittle calcular_doolittle_cpp(vector<vector<double>> A, vector<double> b) {
    ResultadoDoolittle res;
    res.exito = false;

    string msg = validar_sistema(A, b);
    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    int n = A.size();

    vector<vector<double>> L(n, vector<double>(n, 0.0));
    vector<vector<double>> U(n, vector<double>(n, 0.0));

    // Factorización A = L * U
    for (int i = 0; i < n; i++) {

        // Diagonal de L con unos
        L[i][i] = 1.0;

        // Fila i de U
        for (int j = i; j < n; j++) {
            double suma = 0;

            for (int k = 0; k < i; k++) {
                suma += L[i][k] * U[k][j];
            }

            U[i][j] = A[i][j] - suma;
        }

        // Pivote de U no debe ser cero
        if (fabs(U[i][i]) < 1e-12) {
            res.error_msg = "Pivote cercano a cero en U[" + to_string(i + 1) + "][" + to_string(i + 1) +
                            "]: la factorización LU sin pivoteo no se puede realizar.";
            return res;
        }

        // Columna i de L
        for (int j = i + 1; j < n; j++) {
            double suma = 0;

            for (int k = 0; k < i; k++) {
                suma += L[j][k] * U[k][i];
            }

            L[j][i] = (A[j][i] - suma) / U[i][i];
        }
    }

    // Sustitución hacia adelante: L * y = b
    vector<double> y(n, 0.0);

    for (int i = 0; i < n; i++) {
        double suma = 0;

        for (int j = 0; j < i; j++) {
            suma += L[i][j] * y[j];
        }

        y[i] = b[i] - suma;   // L[i][i] = 1
    }

    // Sustitución hacia atrás: U * x = y
    vector<double> x(n, 0.0);

    for (int i = n - 1; i >= 0; i--) {
        double suma = 0;

        for (int j = i + 1; j < n; j++) {
            suma += U[i][j] * x[j];
        }

        x[i] = (y[i] - suma) / U[i][i];
    }

    res.exito = true;
    res.L = L;
    res.U = U;
    res.y = y;
    res.x = x;

    return res;
}

// ----------------------------- FACTORIZACIÓN LU: CROUT -----------------------
// Diagonal de U = 1

struct ResultadoCrout {
    bool exito;
    string error_msg;
    vector<vector<double>> L;
    vector<vector<double>> U;
    vector<double> y;
    vector<double> x;
};

ResultadoCrout calcular_crout_cpp(vector<vector<double>> A, vector<double> b) {
    ResultadoCrout res;
    res.exito = false;

    string msg = validar_sistema(A, b);
    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    int n = A.size();

    vector<vector<double>> L(n, vector<double>(n, 0.0));
    vector<vector<double>> U(n, vector<double>(n, 0.0));

    // Factorización A = L * U
    for (int j = 0; j < n; j++) {

        // Diagonal de U con unos
        U[j][j] = 1.0;

        // Columna j de L
        for (int i = j; i < n; i++) {
            double suma = 0;

            for (int k = 0; k < j; k++) {
                suma += L[i][k] * U[k][j];
            }

            L[i][j] = A[i][j] - suma;
        }

        // Pivote de L no debe ser cero
        if (fabs(L[j][j]) < 1e-12) {
            res.error_msg = "Pivote cercano a cero en L[" + to_string(j + 1) + "][" + to_string(j + 1) +
                            "]: la factorización LU sin pivoteo no se puede realizar.";
            return res;
        }

        // Fila j de U
        for (int i = j + 1; i < n; i++) {
            double suma = 0;

            for (int k = 0; k < j; k++) {
                suma += L[j][k] * U[k][i];
            }

            U[j][i] = (A[j][i] - suma) / L[j][j];
        }
    }

    // Sustitución hacia adelante: L * y = b
    vector<double> y(n, 0.0);

    for (int i = 0; i < n; i++) {
        double suma = 0;

        for (int j = 0; j < i; j++) {
            suma += L[i][j] * y[j];
        }

        y[i] = (b[i] - suma) / L[i][i];
    }

    // Sustitución hacia atrás: U * x = y
    vector<double> x(n, 0.0);

    for (int i = n - 1; i >= 0; i--) {
        double suma = 0;

        for (int j = i + 1; j < n; j++) {
            suma += U[i][j] * x[j];
        }

        x[i] = y[i] - suma;   // U[i][i] = 1
    }

    res.exito = true;
    res.L = L;
    res.U = U;
    res.y = y;
    res.x = x;

    return res;
}


// ============================================================================
// UNIDAD 5: EIGENVALORES Y EIGENVECTORES (Potencia, Potencia Inversa)
// ============================================================================

// Fila de la tabla (compartida por Potencia y Potencia Inversa)
struct FilaPotencia {
    int k;
    vector<double> xn;      // Vector Xn de entrada
    vector<double> yn;      // Vector Yn = A*Xn (o B*Xn en inversa)
    double cn;              // Factor de normalización Cn (mayor en valor absoluto)
    vector<double> x_next;  // Vector normalizado Xn+1 = Yn / Cn
    double lambda_k;        // Aproximación del eigenvalor (λmax o λmin)
    double error;           // En la iteración 1 vale 0
};

// -------------------------- MÉTODO DE LA POTENCIA ---------------------------

struct ResultadoPotencia {
    bool exito;
    double lambda_max;
    vector<double> v_propio;
    int iteraciones;
    string error_msg;
    vector<FilaPotencia> tabla;
};

ResultadoPotencia calcular_potencia_cpp(vector<vector<double>> A, vector<double> x0, double tol, int max_iter) {
    ResultadoPotencia res;
    res.exito = false;
    res.lambda_max = 0;
    res.iteraciones = 0;

    // Validaciones
    string msg = validar_matriz_cuadrada(A);
    if (msg.empty()) msg = validar_vector(x0, A.size(), "x0");

    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    if (tol <= 0) {
        res.error_msg = "La tolerancia debe ser positiva.";
        return res;
    }

    if (max_iter <= 0) {
        res.error_msg = "El máximo de iteraciones debe ser positivo.";
        return res;
    }

    int n = A.size();
    vector<double> x = x0;
    vector<double> y(n, 0.0);
    double lambda = 0;
    double anterior = 0;

    for (int k = 1; k <= max_iter; k++) {
        vector<double> x_actual = x; // Guardar Xn antes de actualizar

        // y = A * x
        for (int i = 0; i < n; i++) {
            y[i] = 0;
            for (int j = 0; j < n; j++) {
                y[i] += A[i][j] * x[j];
            }
        }

        // Componente de mayor valor absoluto (Cn)
        double mayor = 0;
        for (int i = 0; i < n; i++) {
            if (fabs(y[i]) > mayor) mayor = fabs(y[i]);
        }

        if (mayor < 1e-12) {
            res.error_msg = "A·x resultó en el vector nulo: el vector inicial es nulo o está en el espacio nulo de A. Prueba con otro x0.";
            return res;
        }

        // Normalizar: Xn+1 = Yn / Cn
        for (int i = 0; i < n; i++) {
            x[i] = y[i] / mayor;
        }

        // Cociente de Rayleigh: lambda = (x·Ax) / (x·x)
        vector<double> ax(n, 0.0);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                ax[i] += A[i][j] * x[j];
            }
        }

        double numerador = 0, denominador = 0;
        for (int i = 0; i < n; i++) {
            numerador += x[i] * ax[i];
            denominador += x[i] * x[i];
        }

        lambda = numerador / denominador;

        double error = (k > 1) ? fabs(lambda - anterior) : 0.0;

        // Guardar fila completa (k-1 para que coincida con n = 0, 1, 2... del Excel)
        res.tabla.push_back({k - 1, x_actual, y, mayor, x, lambda, error});

        res.lambda_max = lambda;
        res.v_propio = x;
        res.iteraciones = k;

        if (k > 1 && error <= tol) {
            res.exito = true;
            return res;
        }

        anterior = lambda;
    }
    
    res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";
    return res;
}

// ----------------------- MÉTODO DE LA POTENCIA INVERSA -----------------------

struct ResultadoPotenciaInversa {
    bool exito;
    double lambda_min;
    vector<double> v_propio;
    int iteraciones;
    string error_msg;
    vector<FilaPotencia> tabla;
};

// Resuelve A * y = b por Gauss-Jordan con pivoteo parcial.
// 'A' y 'b' se pasan por valor a propósito: se trabaja sobre copias.
// Devuelve false si la matriz es singular.
bool resolver_sistema(vector<vector<double>> A, vector<double> b, vector<double>& y) {
    int n = A.size();
    y.assign(n, 0.0);

    // Matriz aumentada
    for (int i = 0; i < n; i++) {
        A[i].push_back(b[i]);
    }

    for (int i = 0; i < n; i++) {

        // Pivoteo parcial
        int fila_max = i;
        double max_val = fabs(A[i][i]);

        for (int k = i + 1; k < n; k++) {
            if (fabs(A[k][i]) > max_val) {
                max_val = fabs(A[k][i]);
                fila_max = k;
            }
        }

        if (max_val < 1e-12) return false;

        if (fila_max != i) swap(A[i], A[fila_max]);

        // Normalizar fila pivote
        double pivote = A[i][i];

        for (int j = i; j <= n; j++) {
            A[i][j] /= pivote;
        }

        // Eliminar en las demás filas
        for (int k = 0; k < n; k++) {
            if (k != i) {
                double factor = A[k][i];

                for (int j = i; j <= n; j++) {
                    A[k][j] -= factor * A[i][j];
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        y[i] = A[i][n];
    }

    return true;
}

ResultadoPotenciaInversa calcular_potencia_inversa_cpp(vector<vector<double>> A, vector<double> v0, double tol, int max_iter) {
    ResultadoPotenciaInversa res;
    res.exito = false;
    res.lambda_min = 0;
    res.iteraciones = 0;

    // Validaciones
    string msg = validar_matriz_cuadrada(A);
    if (msg.empty()) msg = validar_vector(v0, A.size(), "v0");

    if (!msg.empty()) {
        res.error_msg = msg;
        return res;
    }

    if (tol <= 0) {
        res.error_msg = "La tolerancia debe ser positiva.";
        return res;
    }

    if (max_iter <= 0) {
        res.error_msg = "El máximo de iteraciones debe ser positivo.";
        return res;
    }

    int n = A.size();
    vector<double> vk = v0;
    double lambda_ant = 0;

    for (int k = 1; k <= max_iter; k++) {
        vector<double> x_actual = vk; // Guardar Xn antes de actualizar

        // En vez de multiplicar por A^-1, se resuelve A * y = vk
        vector<double> y;

        if (!resolver_sistema(A, vk, y)) {
            res.error_msg = "La matriz A es singular (no invertible): det(A) = 0, el método no se puede aplicar.";
            return res;
        }

        // Componente de mayor valor absoluto de y (se conserva su signo)
        double max_val = 0;
        double mu = 0;

        for (int i = 0; i < n; i++) {
            if (fabs(y[i]) > max_val) {
                max_val = fabs(y[i]);
                mu = y[i];
            }
        }

        if (max_val < 1e-12) {
            res.error_msg = "Se obtuvo el vector nulo: el vector inicial v0 no puede ser nulo.";
            return res;
        }

        // El eigenvalor de A es el inverso de mu
        double lambda_k = 1.0 / mu;

        // Normalizar el vector
        for (int i = 0; i < n; i++) {
            vk[i] = y[i] / mu;
        }

        double error = 0;
        if (k > 1) error = fabs(lambda_k - lambda_ant);

        // Registrar fila completa: n = k - 1, Xn, Yn, Cn = mu, Xn+1 = vk, lambda_k, error
        res.tabla.push_back({k - 1, x_actual, y, mu, vk, lambda_k, error});

        res.lambda_min = lambda_k;
        res.v_propio = vk;
        res.iteraciones = k;

        if (k > 1 && error < tol) {
            res.exito = true;
            return res;
        }

        lambda_ant = lambda_k;
    }

    res.error_msg = "No se alcanzó la tolerancia en el máximo de iteraciones.";
    return res;
}

// ============================================================================
// VINCULACIÓN EMBIND PARA WEBASSEMBLY (único bloque para todo el módulo)
// ============================================================================
EMSCRIPTEN_BINDINGS(modulo_metodos) {

    // ---------------------------- Contenedores base ----------------------------
    register_vector<double>("VectorDouble");
    register_vector<vector<double>>("VectorVectorDouble");

    // ---------------------------- UNIDAD 2 ----------------------------
    // Bisección
    value_object<FilaBiseccion>("FilaBiseccion")
        .field("k", &FilaBiseccion::k)
        .field("a", &FilaBiseccion::a)
        .field("b", &FilaBiseccion::b)
        .field("xr", &FilaBiseccion::xr)
        .field("fa", &FilaBiseccion::fa)
        .field("fb", &FilaBiseccion::fb)
        .field("fc", &FilaBiseccion::fc)
        .field("error", &FilaBiseccion::error);

    register_vector<FilaBiseccion>("VectorFilaBiseccion");

    value_object<ResultadoBiseccion>("ResultadoBiseccion")
        .field("exito", &ResultadoBiseccion::exito)
        .field("raiz", &ResultadoBiseccion::raiz)
        .field("iteraciones", &ResultadoBiseccion::iteraciones)
        .field("error_final", &ResultadoBiseccion::error_final)
        .field("error_msg", &ResultadoBiseccion::error_msg)
        .field("tabla", &ResultadoBiseccion::tabla);

    emscripten::function("calcular_biseccion_cpp", &calcular_biseccion_cpp);

    // Newton-Raphson
    value_object<FilaIteracion>("FilaIteracion")
        .field("k", &FilaIteracion::k)
        .field("xk", &FilaIteracion::xk)
        .field("fx", &FilaIteracion::fx)
        .field("dfx", &FilaIteracion::dfx)
        .field("error", &FilaIteracion::error);

    register_vector<FilaIteracion>("VectorFilas");

    value_object<ResultadoNewton>("ResultadoNewton")
        .field("exito", &ResultadoNewton::exito)
        .field("raiz", &ResultadoNewton::raiz)
        .field("iteraciones", &ResultadoNewton::iteraciones)
        .field("error_msg", &ResultadoNewton::error_msg)
        .field("tabla", &ResultadoNewton::tabla);

    emscripten::function("calcular_newton_cpp", &calcular_newton_cpp);

    // Secante
    value_object<FilaIteracionSecante>("FilaIteracionSecante")
        .field("k", &FilaIteracionSecante::k)
        .field("x_prev", &FilaIteracionSecante::x_prev)
        .field("xk", &FilaIteracionSecante::xk)
        .field("fx_prev", &FilaIteracionSecante::fx_prev)
        .field("fx", &FilaIteracionSecante::fx)
        .field("error", &FilaIteracionSecante::error);

    register_vector<FilaIteracionSecante>("VectorFilaSecante");

    value_object<ResultadoSecante>("ResultadoSecante")
        .field("exito", &ResultadoSecante::exito)
        .field("raiz", &ResultadoSecante::raiz)
        .field("iteraciones", &ResultadoSecante::iteraciones)
        .field("error_msg", &ResultadoSecante::error_msg)
        .field("tabla", &ResultadoSecante::tabla);

    emscripten::function("calcular_secante_cpp", &calcular_secante_cpp);

    // ---------------------------- UNIDAD 3 ----------------------------
    // Gauss-Jordan
    value_object<PasoGaussJordan>("PasoGaussJordan")
        .field("paso", &PasoGaussJordan::paso)
        .field("descripcion", &PasoGaussJordan::descripcion)
        .field("matriz_estado", &PasoGaussJordan::matriz_estado);

    register_vector<PasoGaussJordan>("VectorPasoGaussJordan");

    value_object<ResultadoGaussJordan>("ResultadoGaussJordan")
        .field("exito", &ResultadoGaussJordan::exito)
        .field("error_msg", &ResultadoGaussJordan::error_msg)
        .field("x", &ResultadoGaussJordan::x)
        .field("pasos", &ResultadoGaussJordan::pasos);

    emscripten::function("calcular_gauss_jordan_cpp", &calcular_gauss_jordan_cpp);

    // Fila compartida por Jacobi y Gauss-Seidel
    value_object<FilaIterativaSis>("FilaIterativaSis")
        .field("k", &FilaIterativaSis::k)
        .field("valores", &FilaIterativaSis::valores)
        .field("error", &FilaIterativaSis::error);

    register_vector<FilaIterativaSis>("VectorFilaIterativa");

    // Jacobi
    value_object<ResultadoJacobi>("ResultadoJacobi")
        .field("exito", &ResultadoJacobi::exito)
        .field("iteraciones", &ResultadoJacobi::iteraciones)
        .field("x_solucion", &ResultadoJacobi::x_solucion)
        .field("tabla", &ResultadoJacobi::tabla)
        .field("error_msg", &ResultadoJacobi::error_msg);

    emscripten::function("calcular_jacobi_cpp", &calcular_jacobi_cpp);

    // Gauss-Seidel
    value_object<ResultadoGaussSeidel>("ResultadoGaussSeidel")
        .field("exito", &ResultadoGaussSeidel::exito)
        .field("iteraciones", &ResultadoGaussSeidel::iteraciones)
        .field("x_solucion", &ResultadoGaussSeidel::x_solucion)
        .field("tabla", &ResultadoGaussSeidel::tabla)
        .field("error_msg", &ResultadoGaussSeidel::error_msg);

    emscripten::function("calcular_gauss_seidel_cpp", &calcular_gauss_seidel_cpp);

    // ---------------------------- UNIDAD 4 ----------------------------
    // Doolittle
    value_object<ResultadoDoolittle>("ResultadoDoolittle")
        .field("exito", &ResultadoDoolittle::exito)
        .field("error_msg", &ResultadoDoolittle::error_msg)
        .field("L", &ResultadoDoolittle::L)
        .field("U", &ResultadoDoolittle::U)
        .field("y", &ResultadoDoolittle::y)
        .field("x", &ResultadoDoolittle::x);

    emscripten::function("calcular_doolittle_cpp", &calcular_doolittle_cpp);

    // Crout
    value_object<ResultadoCrout>("ResultadoCrout")
        .field("exito", &ResultadoCrout::exito)
        .field("error_msg", &ResultadoCrout::error_msg)
        .field("L", &ResultadoCrout::L)
        .field("U", &ResultadoCrout::U)
        .field("y", &ResultadoCrout::y)
        .field("x", &ResultadoCrout::x);

    emscripten::function("calcular_crout_cpp", &calcular_crout_cpp);

    // ---------------------------- UNIDAD 5 ----------------------------
    // Fila compartida por Potencia y Potencia Inversa
    value_object<FilaPotencia>("FilaPotencia")
        .field("k", &FilaPotencia::k)
        .field("xn", &FilaPotencia::xn)
        .field("yn", &FilaPotencia::yn)
        .field("cn", &FilaPotencia::cn)
        .field("x_next", &FilaPotencia::x_next)
        .field("lambda_k", &FilaPotencia::lambda_k)
        .field("error", &FilaPotencia::error);

    register_vector<FilaPotencia>("VectorFilaPotencia");

    // Potencia
    value_object<ResultadoPotencia>("ResultadoPotencia")
        .field("exito", &ResultadoPotencia::exito)
        .field("lambda_max", &ResultadoPotencia::lambda_max)
        .field("v_propio", &ResultadoPotencia::v_propio)
        .field("iteraciones", &ResultadoPotencia::iteraciones)
        .field("error_msg", &ResultadoPotencia::error_msg)
        .field("tabla", &ResultadoPotencia::tabla);

    emscripten::function("calcular_potencia_cpp", &calcular_potencia_cpp);

    // Potencia Inversa
    value_object<ResultadoPotenciaInversa>("ResultadoPotenciaInversa")
        .field("exito", &ResultadoPotenciaInversa::exito)
        .field("lambda_min", &ResultadoPotenciaInversa::lambda_min)
        .field("v_propio", &ResultadoPotenciaInversa::v_propio)
        .field("iteraciones", &ResultadoPotenciaInversa::iteraciones)
        .field("error_msg", &ResultadoPotenciaInversa::error_msg)
        .field("tabla", &ResultadoPotenciaInversa::tabla);

    emscripten::function("calcular_potencia_inversa_cpp", &calcular_potencia_inversa_cpp);
}
