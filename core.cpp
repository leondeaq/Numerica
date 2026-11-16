#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <emscripten/bind.h>

// Prueba de compilacion

// Namespaces para simplificar el cÃ³digo sin prefijos std:: o emscripten::
using namespace std;
using namespace emscripten;

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

// Evaluador parser simple y dinÃ¡mico para f(x)
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
                throw runtime_error("FunciÃ³n no soportada: " + name);
            }
            throw runtime_error("Variable no identificada: " + name);
        }
        throw runtime_error("Sintaxis matemÃ¡tica invÃ¡lida.");
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
                if (abs(right) < 1e-15) throw runtime_error("DivisiÃ³n entre cero.");
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
        if (expr.empty()) throw runtime_error("La funciÃ³n estÃ¡ vacÃ­a.");
        double res = parse_expression();
        if (peek() != '\0') throw runtime_error("Error de sintaxis en la expresiÃ³n.");
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
    double h = 1e-7; // Paso para derivada numÃ©rica centrada

    try {
        // Verificar validez inicial de la expresiÃ³n
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

// VinculaciÃ³n Embind para WebAssembly
EMSCRIPTEN_BINDINGS(modulo_metodos) {
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
}
