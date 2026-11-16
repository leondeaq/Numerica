#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <emscripten/bind.h>

// Namespaces para simplificar el código sin prefijos std:: o emscripten::
using namespace std;
using namespace emscripten;

// ==========================================================
// ESTRUCTURAS - MÉTODO DE NEWTON
// ==========================================================

// Estructura de fila para la tabla de iteraciones
struct FilaIteracion {
    int k;
    double xk;
    double f_xk;
    double df_xk;
    double error;
};

// Estructura para la respuesta enviada a JavaScript
struct ResultadoNewton {
    bool exito;
    double raiz;
    int iteraciones;
    string error_msg;
    vector<FilaIteracion> tabla;
};


// ==========================================================
// ESTRUCTURAS - MÉTODO DE LA POTENCIA
// ==========================================================

struct FilaIteracionPotencia {
    int k;
    double eigenvalor;
    double error;
};

struct ResultadoPotencia {
    bool exito;
    double eigenvalor;
    vector<double> eigenvector;
    int iteraciones;
    string error_msg;
    vector<FilaIteracionPotencia> tabla;
};

// ==========================================================
// LÓGICA Y PARSER MATEMÁTICO - MÉTODO DE NEWTON
// ==========================================================

// Evaluador parser simple y dinámico para f(x)
class EvaluadorMatematico {
private:
    string expr;
    size_t pos;
    double val_x;

    char peek() {
        while (pos < expr.length() && isspace(expr[pos])) pos++;
        if (pos >= expr.length()) return '\0';
        return expr[pos];
    }

    char get() {
        char c = peek();
        if (c != '\0') pos++;
        return c;
    }

    double parse_number() {
        size_t start = pos;
        while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) {
            pos++;
        }
        string num_str = expr.substr(start, pos - start);
        return stod(num_str);
    }

    double parse_factor() {
        char c = peek();
        if (c == '-') {
            get();
            return -parse_factor();
        }
        if (c == '+') {
            get();
            return parse_factor();
        }
        if (c == '(') {
            get();
            double val = parse_expression();
            if (peek() == ')') get();
            return val;
        }
        if (isdigit(c) || c == '.') {
            return parse_number();
        }
        if (isalpha(c)) {
            size_t start = pos;
            while (pos < expr.length() && isalpha(expr[pos])) pos++;
            string name = expr.substr(start, pos - start);

            if (name == "x" || name == "X") {
                return val_x;
            }
            if (name == "pi" || name == "PI") {
                return 3.14159265358979323846;
            }
            if (name == "e" || name == "E") {
                return 2.71828182845904523536;
            }

            if (peek() == '(') {
                get();
                double arg = parse_expression();
                if (peek() == ')') get();

                if (name == "sin") return sin(arg);
                if (name == "cos") return cos(arg);
                if (name == "tan") return tan(arg);
                if (name == "sqrt") return sqrt(arg);
                if (name == "exp") return exp(arg);
                if (name == "log" || name == "ln") return log(arg);
                if (name == "abs") return abs(arg);
                throw runtime_error("Función no soportada: " + name);
            }
            throw runtime_error("Variable no identificada: " + name);
        }
        throw runtime_error("Sintaxis matemática inválida.");
    }

    double parse_power() {
        double base = parse_factor();
        while (peek() == '^') {
            get();
            double exp_val = parse_factor();
            base = pow(base, exp_val);
        }
        return base;
    }

    double parse_term() {
        double left = parse_power();
        while (true) {
            char c = peek();
            if (c == '*') {
                get();
                left *= parse_power();
            } else if (c == '/') {
                get();
                double right = parse_power();
                if (abs(right) < 1e-15) throw runtime_error("División entre cero.");
                left /= right;
            } else {
                break;
            }
        }
        return left;
    }

    double parse_expression() {
        double left = parse_term();
        while (true) {
            char c = peek();
            if (c == '+') {
                get();
                left += parse_term();
            } else if (c == '-') {
                get();
                left -= parse_term();
            } else {
                break;
            }
        }
        return left;
    }

public:
    EvaluadorMatematico(string e) : expr(e), pos(0), val_x(0) {}

    double evaluar(double x) {
        pos = 0;
        val_x = x;
        if (expr.empty()) throw runtime_error("La función está vacía.");
        double res = parse_expression();
        if (peek() != '\0') throw runtime_error("Error de sintaxis en la expresión.");
        return res;
    }
};

double evaluar_f(string expr, double x) {
    EvaluadorMatematico eval(expr);
    return eval.evaluar(x);
}

ResultadoNewton calcular_newton_cpp(string expr_str, double x0, double tol, int max_iter) {
    ResultadoNewton res;
    res.exito = false;

    double xk = x0;
    double h = 1e-7; // Paso para derivada numérica centrada

    try {
        // Verificar validez inicial de la expresión
        evaluar_f(expr_str, xk);

        for (int k = 1; k <= max_iter; ++k) {
            double f_xk = evaluar_f(expr_str, xk);
            double df_xk = (evaluar_f(expr_str, xk + h) - evaluar_f(expr_str, xk - h)) / (2.0 * h);

            if (abs(df_xk) < 1e-12) {
                res.error_msg = "La derivada f'(x) es muy cercana a cero.";
                return res;
            }

            double x_next = xk - (f_xk / df_xk);
            double error = (abs(x_next) > 1e-12) ? abs((x_next - xk) / x_next) : abs(x_next - xk);

            res.tabla.push_back({k, xk, f_xk, df_xk, error});

            if (error < tol) {
                res.exito = true;
                res.raiz = x_next;
                res.iteraciones = k;
                return res;
            }
            xk = x_next;
        }

        res.exito = true;
        res.raiz = xk;
        res.iteraciones = max_iter;
        return res;
    } catch (const exception& e) {
        res.error_msg = e.what();
        return res;
    }
}

// ==========================================================
// LÓGICA - MÉTODO DE LA POTENCIA
// ==========================================================

ResultadoPotencia calcular_potencia_cpp(int n, vector<vector<double>> A, vector<double> x, double tolerancia, int maxIter) {
    ResultadoPotencia res;
    res.exito = false;
    res.iteraciones = 0;
    res.eigenvalor = 0.0;
    
    // Validaciones de seguridad para dimensiones
    if (n < 1) {
        res.error_msg = "Tamano no valido";
        return res;
    }
    if (A.size() != static_cast<size_t>(n) || x.size() != static_cast<size_t>(n)) {
        res.error_msg = "Las dimensiones de la matriz o el vector no coinciden con 'n'.";
        return res;
    }

    vector<double> y(n, 0.0);
    double mayor, lambda = 0.0, anterior = 0.0;
    double error = 0.0, numerador, denominador;

    for (int iter = 1; iter <= maxIter; iter++) {
        
        // Multiplicación de la matriz A por el vector actual x: y = A * x
        for (int i = 0; i < n; i++) {
            y[i] = 0;
            for (int j = 0; j < n; j++) {
                y[i] += A[i][j] * x[j];
            }
        }

        // Buscar el elemento con mayor valor absoluto en el vector 'y'
        mayor = 0;
        for (int i = 0; i < n; i++) {
            if (fabs(y[i]) > mayor) {
                mayor = fabs(y[i]);
            }
        }

        // Validación para evitar divisiones entre cero si se genera un vector nulo
        if (mayor == 0) {
            res.error_msg = "No se puede continuar: vector nulo.";
            return res;
        }

        // Normalizar el vector: x = y / mayor
        for (int i = 0; i < n; i++) {
            x[i] = y[i] / mayor;
        }

        // Calcular nuevamente A * x usando el vector ya normalizado
        for (int i = 0; i < n; i++) {
            y[i] = 0;
            for (int j = 0; j < n; j++) {
                y[i] += A[i][j] * x[j];
            }
        }

        // Aplicación del Cociente de Rayleigh para calcular el eigenvalor (lambda)
        numerador = 0;
        denominador = 0;
        for (int i = 0; i < n; i++) {
            numerador += x[i] * y[i];
            denominador += x[i] * x[i];
        }

        if (denominador == 0) {
            res.error_msg = "División por cero al calcular el cociente de Rayleigh.";
            return res;
        }

        lambda = numerador / denominador;

        // Cálculo del error aproximado a partir de la segunda iteración
        if (iter > 1) {
            error = fabs(lambda - anterior);
        } else {
            error = 0.0;
        }

        // Guardar fila para la tabla web
        res.tabla.push_back({iter, lambda, error});

        // Criterio de parada
        if (iter > 1 && error <= tolerancia) {
            res.exito = true;
            res.eigenvalor = lambda;
            res.eigenvector = x;
            res.iteraciones = iter;
            return res;
        }

        anterior = lambda;
    }

    res.exito = false;
    res.error_msg = "Se alcanzo el maximo de iteraciones sin lograr la tolerancia deseada.";
    res.eigenvalor = lambda;
    res.eigenvector = x;
    res.iteraciones = maxIter;
    
    return res;
}

// ==========================================================
// VINCULACIÓN EMBIND (WebAssembly)
// ==========================================================

EMSCRIPTEN_BINDINGS(modulo_metodos) {
    // ------------------------------------
    // BINDINGS NEWTON
    // ------------------------------------
    value_object<FilaIteracion>("FilaIteracion")
        .field("k", &FilaIteracion::k)
        .field("xk", &FilaIteracion::xk)
        .field("f_xk", &FilaIteracion::f_xk)
        .field("df_xk", &FilaIteracion::df_xk)
        .field("error", &FilaIteracion::error);

    value_object<ResultadoNewton>("ResultadoNewton")
        .field("exito", &ResultadoNewton::exito)
        .field("raiz", &ResultadoNewton::raiz)
        .field("iteraciones", &ResultadoNewton::iteraciones)
        .field("error_msg", &ResultadoNewton::error_msg)
        .field("tabla", &ResultadoNewton::tabla);

    register_vector<FilaIteracion>("VectorFilas");
    emscripten::function("calcular_newton_cpp", &calcular_newton_cpp);

    // ------------------------------------
    // BINDINGS POTENCIA
    // ------------------------------------
    value_object<FilaIteracionPotencia>("FilaIteracionPotencia")
        .field("k", &FilaIteracionPotencia::k)
        .field("eigenvalor", &FilaIteracionPotencia::eigenvalor)
        .field("error", &FilaIteracionPotencia::error);

    value_object<ResultadoPotencia>("ResultadoPotencia")
        .field("exito", &ResultadoPotencia::exito)
        .field("eigenvalor", &ResultadoPotencia::eigenvalor)
        .field("eigenvector", &ResultadoPotencia::eigenvector)
        .field("iteraciones", &ResultadoPotencia::iteraciones)
        .field("error_msg", &ResultadoPotencia::error_msg)
        .field("tabla", &ResultadoPotencia::tabla);

    // Tipos std::vector nativos que se deben exportar a JS para el Método de la Potencia
    register_vector<double>("VectorDouble");
    register_vector<vector<double>>("VectorMatriz");
    register_vector<FilaIteracionPotencia>("VectorFilasPotencia");

    emscripten::function("calcular_potencia_cpp", &calcular_potencia_cpp);
}
