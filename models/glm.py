import math
import numpy as np

def sigmoid(x):
    return 1.0/(1.0+math.exp(-x))

sigmoidv = np.vectorize(sigmoid)

class GLM:
    def __init__(self, features, params, binary=False):
        self.features = features
        self.params = params
        self.binary = binary

        self.W = params[:,1:].T
        self.bias = params[0,0].T

    def predict(self, X):
        y = np.matmul(X, self.W)[:,0] + self.bias
        return sigmoidv(y) if self.binary else y
